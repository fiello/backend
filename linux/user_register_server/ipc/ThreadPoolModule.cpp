
//native
#include "ThreadPoolModule.h"
#include "Logger.h"
//thirdparty
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#include <boost/mem_fn.hpp>
#include <boost/thread/locks.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using namespace trc;

namespace IPC
{
	using namespace boost::threadpool;
	boost::shared_mutex CBaseTask::m_sharedFileAccess;
	boost::condition_variable CBaseTask::m_condMaintenance;
	boost::mutex CBaseTask::m_mutexMaintenance;

	//////////////////////////////////////////////////////////////////////////
	//terminating symbol
	const std::string g_strRequestTerminator = "\r\n";
	//max length of the username
	const size_t g_iMaxUsernameLength = 160;
	//max message length
	const int g_iMaxMessageLength = 4*g_iMaxBufferSize;
	//username tag
	const std::string g_strUsernameTag = "username";
	//email tag
	const std::string g_strEmailTag = "email";
	//max records in a file
	const int g_iMaxRecords = 100;


	//struct with the requests type
	static const char* g_szRequestNames[] =
	{
		"REGISTER",
		"GET"
	};

	enum etRequestID
	{
		eREQUEST_UNDEFINED = - 1,
		//request ID according to the names in global array
		eREQUEST_REGISTER,
		eREQUEST_GET,

		eREQUEST_COUNT
	};

	typedef enum _etResponseCode
	{
		eRESPONSE_UNDEFINED = - 1,
		//request ID according to the names in global array
		eRESPONSE_OK = 200,
		eRESPONSE_BAD_REQUEST = 400,
		eRESPONSE_NOT_FOUND = 404,
		eRESPONSE_OVERLOADED = 405,
		eRESPONSE_NOT_ACCEPTABLE = 406,
		eRESPONSE_CONFLICT = 409,
		eRESPONSE_SERVICE_UNAVAILABLE = 503
	} eResponseCode;


	//////////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded constructor for the socket wrapper class
	 * @param iSocketDesc - in, socket descriptor
	 */
	CBaseSocket::CBaseSocket(int iSocketDesc)
	{
		try
		{
			if (iSocketDesc <= 0)
			{
				LOGERROR << "Socket for the incoming connection is invalid";
				throw -1;
			}
			m_iSocketDescriptor = iSocketDesc;
			LOGDEBUG << "Processing connection, socket=" << m_iSocketDescriptor;
		}
		CATCH_THROW(-1)
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Destructor for the socket wrapper class, perform shutdown and closure for
	 * the internal socket descriptor
	 */
	CBaseSocket::~CBaseSocket()
	{
		try
		{
			LOGDEBUG << "Erasing " << m_iSocketDescriptor;
			if (IsConnected() && (shutdown(m_iSocketDescriptor,SHUT_RDWR) == -1) )
				LOGERROR << "Error shutting down socket, err=" << errno;
			if (close(m_iSocketDescriptor) == -1)
				LOGERROR << "Error closing socket, err=" << errno;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Helper function to understand if (in case of a connection-oriented socket) the socket
	 * was ever connected to another endpoint or not. This will help avoiding the issue with 'shutdown'
	 * of a non-connected socket
	 * @return bool - true is 'connect' operation was ever called for the socket, false otherwise
	 */
	bool CBaseSocket::IsConnected()
	{
		try
		{
			char szBuffer[g_iMaxBufferSize] = {0};
			int iByteCount = 0;
			if ( (iByteCount = recv(m_iSocketDescriptor,szBuffer,g_iMaxBufferSize,MSG_DONTWAIT|MSG_PEEK)) == -1 &&
					errno == ENOTCONN )
			{
				 return false;
			}
		}
		CATCH
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main function that performs data parsing, reading from data file and writing to it,
	 * generating response messages to the client. In case of TCP connection this function is called
	 * from the worker thread function. In case of UDP connection this function appears to be the
	 * core thread function
	 * @param bIsUDPSocket - flag that we are in UDP stream
	 */
	void CBaseTask::ProcessData(bool bIsUDPSocket)
	{
		try
		{
			try
			{
				//in case of maintenance mode this function freezes the whole thread
				//unless process is killed or maintenance mode is not turned off via command line option
				if (m_ptrThreadPool->GetMaintenance())
				{
					boost::unique_lock<boost::mutex> lock(m_mutexMaintenance);
					m_condMaintenance.wait(lock);
				}
			}
			CATCH

			//need local time to maintain the 'sleep' interval before sending data back to the client
			boost::posix_time::ptime timeStart = boost::get_system_time();
			int iTimeout = m_ptrThreadPool->GetSendTimeout();
			std::string strResponseMessage("");
			m_strDataRegisterFile = m_ptrThreadPool->GetDataPath();

			//Let's try parsing the string
			// 1. Find termination symbol
			size_t posTerm = m_strMsgBuffer.find(g_strRequestTerminator);
			if (posTerm == std::string::npos)
			{
				LOGDEBUG << "Message contains no termination";
				//no termination symbol? ok, let's check message bound and respond with Bad Request if
				//mesage length is exceeded
				if (m_strMsgBuffer.length() > g_iMaxMessageLength*2)
				{
					strResponseMessage = m_ptrThreadPool->GetResponseMessage(eRESPONSE_BAD_REQUEST);
					m_strMsgBuffer.clear();
					SleepBeforeSend(timeStart,iTimeout);
					if (bIsUDPSocket)
					{
						if (sendto(m_iSocketDesc,strResponseMessage.c_str(),strResponseMessage.length(),0,(sockaddr*)&m_udpClient,sizeof(m_udpClient)) == -1)
						{
							LOGERROR << "Error #1 while sending data to the remote UDP socket ("<<m_iSocketDesc<<"), err="<<errno;
						}
					}
					else
					{
						if (write(m_iSocketDesc,strResponseMessage.c_str(),strResponseMessage.length()) == -1)
						{
							LOGERROR << "Error #2 while writing data to the remote TCP socket ("<<m_iSocketDesc<<"), err="<<errno;
						}
					}
				}
			}
			else
			{
				// 2. Once termination symbol is found let's parse
				LOGDEBUG << "Termination found";
				std::list<std::string> strListRequests;
				std::string strTempMessage("");
				eResponseCode respCode = eRESPONSE_UNDEFINED;
				int iRequestIndex = -1;
				bool bStopParsing = false;
				std::string strUsername(""), strEmail("");
				while (posTerm != std::string::npos)
				{
					//capture temp message, starting from zero position up to termination symbol
					posTerm += g_strRequestTerminator.length();
					strTempMessage = m_strMsgBuffer.substr(0,posTerm);
					m_strMsgBuffer.erase(0,posTerm);
					posTerm = m_strMsgBuffer.find(g_strRequestTerminator);

					if (m_spProcessData->Parse(strTempMessage))
					{
						//request seems to be parsed, let's traverse through the tree of params-values to see
						//if parser got something useful for us
						for (iRequestIndex = eREQUEST_UNDEFINED+1; iRequestIndex < eREQUEST_COUNT && !bStopParsing; ++iRequestIndex)
						{
							//each request should have different logic in terms of parsing the file
							switch (iRequestIndex)
							{
								case eREQUEST_REGISTER:
									try
									{
										if (m_spProcessData->GetValue(g_szRequestNames[iRequestIndex],g_strUsernameTag.c_str(),strUsername) &&
											m_spProcessData->GetValue(g_szRequestNames[iRequestIndex],g_strEmailTag.c_str(),strEmail) )
										{
											bStopParsing = true;
											boost::trim(strUsername);
											boost::trim(strEmail);
											if (strUsername.empty() || strEmail.empty() || (strUsername.length() > g_iMaxUsernameLength) ||
													strEmail.find('@') == std::string::npos)
											{
												respCode = eRESPONSE_NOT_ACCEPTABLE;
												break;
											}

											std::list<std::string> listRecords;
											int iLines = ReadFile(listRecords);

											if (iLines == g_iMaxRecords)
											{
												respCode = eRESPONSE_OVERLOADED;
												break;
											}

											if ( iLines == -1)
											{
												respCode = eRESPONSE_SERVICE_UNAVAILABLE;
												break;
											}

											bool bIsPresent = false;
											strUsername += ";";
											BOOST_FOREACH(const std::string& strTemp, listRecords)
											{
												if (strTemp.find(strUsername) != std::string::npos)
												{
													bIsPresent = true;
													break;
												}
											}

											if (!bIsPresent)
											{
												boost::lock_guard<boost::shared_mutex> lock(m_sharedFileAccess);
												std::ofstream dataOut(m_strDataRegisterFile, std::fstream::app);
												if (dataOut.good())
												{
													dataOut << strUsername << strEmail << std::endl;
													respCode = eRESPONSE_OK;
												}
												else
												{
													LOGERROR << "Error while writing to file: "<< m_strDataRegisterFile;
													respCode = eRESPONSE_SERVICE_UNAVAILABLE;
												}
											}
											else
											{
												respCode = eRESPONSE_CONFLICT;
											}
											++m_ulSuccededParsing;
										} // if (m_spProcessData->GetValue ...
									}
									catch(...)
									{
										LOGERROR << "Exception while processing request id: "<< iRequestIndex;
										respCode = eRESPONSE_SERVICE_UNAVAILABLE;
									}
									break;
								case eREQUEST_GET:
									try
									{
										if (m_spProcessData->GetValue(g_szRequestNames[iRequestIndex],g_strUsernameTag.c_str(),strUsername))
										{
											bStopParsing = true;
											boost::trim(strUsername);
											if (strUsername.empty() || (strUsername.length() > g_iMaxUsernameLength))
											{
												respCode = eRESPONSE_NOT_ACCEPTABLE;
												break;
											}

											++m_ulSuccededParsing;
											std::list<std::string> listRecords;
											int iLines = ReadFile(listRecords);

											if ( iLines == -1)
											{
												respCode = eRESPONSE_SERVICE_UNAVAILABLE;
												break;
											}

											bool bIsPresent = false;
											strUsername += ";";
											BOOST_FOREACH(const std::string& strTemp, listRecords)
											{
												if (strTemp.find(strUsername) != std::string::npos)
												{
													bIsPresent = true;
													strEmail = strTemp.substr(strUsername.length(),strTemp.length()-strUsername.length());
													break;
												}
											}

											if (bIsPresent)
											{
												respCode = eRESPONSE_OK;
											}
											else
											{
												respCode = eRESPONSE_NOT_FOUND;
											}

											++m_ulSuccededParsing;
										}
									}
									catch(...)
									{
										LOGERROR << "Exception while processing request id: "<< iRequestIndex;
										respCode = eRESPONSE_SERVICE_UNAVAILABLE;
									}
									break;
								default:
								{
									LOGERROR << "Trying to handle unknown request type";
									respCode = eRESPONSE_BAD_REQUEST;
								}
							} // switch (iRequestIndex)
						} // for (iRequestIndex = ...
					} //if (m_spProcessData->Parse(strTempMessage))

					if (respCode == eRESPONSE_UNDEFINED)
					{
						//this means none of parsing rules was applied to the code
						LOGWARN << "Unacceptable request";
						respCode = eRESPONSE_NOT_ACCEPTABLE;
					}

					strResponseMessage = m_ptrThreadPool->GetResponseMessage(respCode);
					LOGDEBUG << "Respond code="<<respCode<<"; message = "<<strResponseMessage;

					if (respCode == eRESPONSE_OK)
					{
						if (iRequestIndex-1 == eREQUEST_GET)
							strResponseMessage += " " + strEmail;
						else
							strResponseMessage += " OK";
					}


					strResponseMessage += g_strRequestTerminator;

					SleepBeforeSend(timeStart,iTimeout);
					//do the 'send' staff and get ready for the new cycle
					if (bIsUDPSocket)
					{
						if (sendto(m_iSocketDesc,strResponseMessage.c_str(),strResponseMessage.length(),0,(sockaddr*)&m_udpClient,sizeof(m_udpClient)) == -1)
						{
							LOGERROR << "Error #2 while sending data to the remote UDP socket ("<<m_iSocketDesc<<"), err="<<errno;
						}
					}
					else
					{
						if (write(m_iSocketDesc,strResponseMessage.c_str(),strResponseMessage.length()) == -1)
						{
							LOGERROR << "Error #2 while sending data to the remote TCP socket ("<<m_iSocketDesc<<"), err="<<errno;
						}
					}

					//prepare for parsing, cleanup for new cycle
					if (posTerm != std::string::npos)
					{
						bStopParsing = false;
						strUsername = "";
						strEmail = "";
						m_spProcessData->Clean();
						strListRequests.clear();
						iRequestIndex = -1;
						strTempMessage = "";
						respCode = eRESPONSE_UNDEFINED;
					}
				} // while (posTerm != std::string::npos)
			} //if (posTerm != std::string::npos) - when we hot terminating symbol
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Small routine to read file and count number of lines from it
	 * @param listRecords - list where the contents of the file will be copied to
	 * @return int - number of lines received from the file
	 */
	int CBaseTask::ReadFile(std::list<std::string>& listRecords)
	{
		int iLinesRead = -1;
		try
		{
			iLinesRead = 0;
			std::string strTemp;

			boost::shared_lock<boost::shared_mutex> lock(m_sharedFileAccess);
			std::ifstream dataIn(m_strDataRegisterFile, std::fstream::in);
			if (dataIn.good())
			{
				while(!dataIn.eof() && (iLinesRead <= g_iMaxRecords))
				{
					std::getline(dataIn,strTemp);
					listRecords.push_back(strTemp);
					++iLinesRead;
				}
				//since eof brings +1 we need to decrement it to get real number of lines
				--iLinesRead;
			}
		}
		catch(...)
		{
			iLinesRead = -1;
		}
		return iLinesRead;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Function to calc and put the thread to sleep if necessary (maintain the 'sleep' period)
	 * @param ptStart - time the message was received and started to be processed
	 * @param iTimeout - timeout we need to wait before sending the message back to client
	 * See if this sleep interval already elapsed and we need to send the data back immediately
	 */
	void CBaseTask::SleepBeforeSend(const boost::posix_time::ptime& ptStart, int iTimeout)
	{
		try
		{
			boost::posix_time::ptime ptWake = ptStart + boost::posix_time::milliseconds(iTimeout);
			boost::posix_time::ptime ptCurrent = boost::get_system_time();
			if (ptWake > ptCurrent)
			{
				boost::this_thread::sleep(ptWake - ptCurrent);
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main routine responsible for reading TCP data from client and
	 * responding back to the client
	 */
	void CTCPReceiveTask::ReceiveData()
	{
		m_bIsTaskCompleted = false;
		try
		{
			if (m_bIsFirstTime)
				LOGDEBUG << "Accepted new TCP connection, socketID="<<GetDescriptor();

			//erase filename every time. In case of maintenance mode filename can be changed, so we have to adopt to this value
			//TODO: slight optimization can be to put data register filename in shared memory and access it from different tasks
			//however this will require synchronization
			m_strDataRegisterFile = "";

			if (m_bPendingDelete)
			{
				LOGWARN << "Incoming event on pending-close task, "<<m_spSocket->GetDescriptor();
				return;
			}

			char szBuffer[g_iMaxBufferSize] = {0};
			int iByteCount = 0;

			if ( (iByteCount = recv(m_spSocket->GetDescriptor(),szBuffer,g_iMaxBufferSize,0)) == -1)
			{
				LOGERROR << "Error while reading from the socket, socketDesc="<<m_spSocket->GetDescriptor()<<", err="<<errno;
			}
			else if (iByteCount == 0)
			{
				m_bPendingDelete = true;
				LOGDEBUG << "Client is dead, remove task and close the socket";

				//check if internal buffer from previous reads is not empty - consider this as bad uncompleted request
				if (!m_strMsgBuffer.empty())
				{
					if (m_ptrThreadPool->GetMaintenance())
					{
						try
						{
							boost::unique_lock<boost::mutex> lock(m_mutexMaintenance);
							m_condMaintenance.wait(lock);
						}
						CATCH
					}

					std::string strResponseMessage("");
					strResponseMessage = m_ptrThreadPool->GetResponseMessage(eRESPONSE_BAD_REQUEST);
					strResponseMessage += g_strRequestTerminator;
					if (write(m_spSocket->GetDescriptor(),strResponseMessage.c_str(),strResponseMessage.length()) == -1)
					{
						LOGERROR << "Error while sending data to the remote socket ("<<m_spSocket->GetDescriptor()<<"), err="<<errno;
					}
					m_strMsgBuffer.erase();
				}
				m_ptrThreadPool->AddPendingRemove(m_spSocket->GetDescriptor());
			}
			else
			{
				LOGDEBUG << "Bytes read: " << iByteCount << ". SocketD:"<<m_spSocket->GetDescriptor();
				int iBufSize = m_strMsgBuffer.length() + iByteCount;
				m_strMsgBuffer.append(szBuffer);
				if (iByteCount != g_iMaxBufferSize)
				{
					//guard-rail from garbage in the socket data (faced several cases when number of bytes
					//read and written to the buffer was different
					m_strMsgBuffer.erase(iBufSize,m_strMsgBuffer.length() - iBufSize);
				}
				ProcessData();
			} //else if ( iByteCount = recv(...) > 0)

			m_bIsTaskCompleted = true;
			m_funcHandler(m_bPendingDelete ? -1 : m_spSocket->GetDescriptor());
		}
		CATCH
		//set it anyway, in case if exception occurred
		m_bIsTaskCompleted = true;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded constructor for the Thread Pool Module. Requires non-zero thread pool size
	 * to initiate it. If incorrect size is specified, a error will be logged and data will be cured
	 * @param iPoolSize - in, pool size
	 * @param strDataRegisterFile - data register file
	 * @param iSendTimeout - timeout we need to maintain before sending data back to the client
	 */
	CThreadPoolModule::CThreadPoolModule(size_t iPoolSize, const std::string& strDataRegisterFile, int iSendTimeout):
		m_strDataRegisterFile(strDataRegisterFile),
		m_iSendTimeout(iSendTimeout)
	{
		try
		{
			m_spThreadPool.reset(new pool());
			m_spThreadPool->size_controller().resize(iPoolSize);

			std::ostringstream out;
			out << eRESPONSE_OK;
			m_mapResponseMessages[eRESPONSE_OK] = out.str();
			out.str("");
			out << eRESPONSE_BAD_REQUEST << " Bad request";
			m_mapResponseMessages[eRESPONSE_BAD_REQUEST] = out.str();
			out.str("");
			out << eRESPONSE_NOT_FOUND << " Not Found";
			m_mapResponseMessages[eRESPONSE_NOT_FOUND] = out.str();
			out.str("");
			out << eRESPONSE_OVERLOADED<< " Overloaded";
			m_mapResponseMessages[eRESPONSE_OVERLOADED] = out.str();
			out.str("");
			out << eRESPONSE_NOT_ACCEPTABLE << " Not Acceptable";
			m_mapResponseMessages[eRESPONSE_NOT_ACCEPTABLE] = out.str();
			out.str("");
			out << eRESPONSE_CONFLICT << " Conflict";
			m_mapResponseMessages[eRESPONSE_CONFLICT] = out.str();
			out.str("");
			out << eRESPONSE_SERVICE_UNAVAILABLE << " Service Unavailable";
			m_mapResponseMessages[eRESPONSE_SERVICE_UNAVAILABLE] = out.str();
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Adding task to the thread pool via assigning tasks to the thread queue
	 * @param spTask - in, reference to the wrapped task to be queued
	 */
	void CThreadPoolModule::AddTask(const spCTCPReceiveTask& spTask)
	{
		try
		{
			{
				boost::lock_guard<boost::shared_mutex> lock(m_sharedTaskAccess);
				m_mapReceiveTasks[spTask->GetDescriptor()] = spTask;
			}
			spTask->AssignParrent(this);
			RenewTask(spTask);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded routine to prepare UDP-specific params and queue a new thread with them
	 * @param iUDPSocket - in, UDP socket number, will be requied for sending data back
	 * @param udpClient - in, ref to a struct which describes the target client we got a message from (port, ip, etc...)
	 * @param strBuffer - in, ref to string with the message that UDP listener read from the socket
	 */
	void CThreadPoolModule::AddTask(int iUDPSocket, const tSockAddr& udpClient, const std::string& strBuffer)
	{
		try
		{
			// The main thread function (CThreadPoolModule::ProcessUDPData) accepts parameters by value (not by reference).
			// This gives some overhead for temporary objects construction but we need because thread should accept
			// unchanged values to start working with - references are not ok in this case. We could create CBaseTask
			m_spThreadPool->schedule(boost::bind(&CThreadPoolModule::ProcessUDPData,this, iUDPSocket, udpClient, strBuffer));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Rescheduling existing task by adding it to the thread queue
	 * @param spTask - in, reference to the wrapped task to be renewed
	 */
	void CThreadPoolModule::RenewTask(const spCTCPReceiveTask& spTask)
	{
		try
		{
			m_spThreadPool->schedule(boost::bind(&CTCPReceiveTask::ReceiveData,spTask));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main thread routine to process UDP data
	 * @param iUDPSocket - in, UDP socket number, will be requied for sending data back
	 * @param udpClient - in, ref to a struct which describes the target client we got a message from (port+ip)
	 * @param strBuffer - in, string with the message that UDP listener read from the socket
	 */
	void CThreadPoolModule::ProcessUDPData(int iUDPSocket, tSockAddr udpClient, std::string strBuffer)
	{
		try
		{
			LOGDEBUG << "Accepting new connection on UDP";
			CBaseTask baseTask(iUDPSocket);
			baseTask.AssignParrent(this);
			baseTask.AssignUDPInfo(udpClient);
			baseTask.AssignMessageBuffer(strBuffer);
			baseTask.ProcessData(true);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Routine to find existing tasks based on the socket descriptor. ThreadPoolModule maintains the
	 * list of all scheduled tasks, either active or passive, therefore we can go through the list
	 * and find existing task there
	 * @param iSocketFD - in, socket file descriptor
	 * @param spTarget - in/out, reference to the wrapped task, will contain non-zero value if task is found
	 * @return bool - true, if task was found in the internal container, false otherwise
	 */
	bool CThreadPoolModule::FindTaskBySocket(int iSocketFD, spCTCPReceiveTask& spTarget)
	{
		try
		{
			boost::shared_lock<boost::shared_mutex> lock(m_sharedTaskAccess);
			tIterMapReceiveTasks iter = m_mapReceiveTasks.find(iSocketFD);
			if (iter != m_mapReceiveTasks.end())
			{
				spTarget = iter->second;
				//reschedule task only if it's completed already
				if (spTarget->IsCompleted())
				{
					return true;
				}
				LOGDEBUG << "Task for socket "<<iSocketFD<<" not completed yet, skip reschedule";
			}
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Routine to find existing tasks based on the socket descriptor and delete it
	 * @param iSocketFD - in, socket file descriptor
	 * @param ulSuccededParsing - in/out number of TCP messages successfully parsed - will be used further to
	 * report some statistics at the end of application
	 * @return - true if routine was successfully deleted from internal containers, false otherwise
	 */
	bool CThreadPoolModule::RemoveTaskBySocket(int iSocketFD, unsigned long& ulSuccededParsing)
	{
		ulSuccededParsing = 0;
		try
		{
			boost::upgrade_lock<boost::shared_mutex> lockShared(m_sharedTaskAccess);
			tIterMapReceiveTasks iter = m_mapReceiveTasks.find(iSocketFD);
			if (iter != m_mapReceiveTasks.end())
			{
				if (!iter->second->IsCompleted())
				{
					LOGWARN << "Attempt to erase uncompleted task("<<iSocketFD<<")";
				}
				else
				{
					LOGDEBUG << "Erasing task for socket = " << iSocketFD;
					boost::lock_guard<boost::recursive_mutex> lockRecursiveDelete(m_RemoveAccess);
					ulSuccededParsing = iter->second->GetParsingStatistics();
					boost::upgrade_to_unique_lock<boost::shared_mutex> lockUnique(lockShared);
					m_mapReceiveTasks.erase(iSocketFD);
					return true;
				}
			}
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Add a socket descriptor to the internal container of sockets that needs to be deleted by the
	 * selector thread
	 * @param iSocketFD - socket descriptor to be added to pending list
	 */
	void CThreadPoolModule::AddPendingRemove(int iSocketFD)
	{
		try
		{
			boost::lock_guard<boost::recursive_mutex> lock(m_RemoveAccess);
			m_listPendingRemoveSockets.push_back(iSocketFD);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Routine to traverse through the internal container with pending sockets and
	 * delete them and tasks assocated with them if possible
	 * @return uLong - summary of succeeded parsings for all tasks
	 *
	 */
	unsigned long CThreadPoolModule::RemovePendingTasks()
	{
		unsigned long ulSuccededParsing = 0;
		try
		{
			boost::lock_guard<boost::recursive_mutex> lock(m_RemoveAccess);
			if (!m_listPendingRemoveSockets.size())
				return ulSuccededParsing;

			std::list<int>::iterator iterDeleter;
			std::list<int>::iterator iterIndexer = m_listPendingRemoveSockets.begin();
			while (iterIndexer != m_listPendingRemoveSockets.end())
			{
				iterDeleter = iterIndexer;
				++iterIndexer;
				if (RemoveTaskBySocket(*iterDeleter,ulSuccededParsing))
				{
					LOGDEBUG << "Erasing socket from pending list";
					m_listPendingRemoveSockets.remove(*iterDeleter);
					return ulSuccededParsing;
				}
			}
		}
		CATCH
		return ulSuccededParsing;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Get response message based on the id of the return code passed in
	 * @param iResponseCode - in, code to find the string for
	 * @return - valid response code message if exists, empty string for invalid response code
	 */
	std::string CThreadPoolModule::GetResponseMessage(int iResponseCode)
	{
		std::string strResponse = "";
		try
		{
			std::map<int,std::string>::iterator iter = m_mapResponseMessages.find(iResponseCode);
			if (iter != m_mapResponseMessages.end())
			{
				strResponse = iter->second;
			}
		}
		CATCH
		return strResponse;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Function to set maintenance mode for the whole pool of tasks (both TCP and UDP)
	 * @param iMaint - maintenance mode, either 0 or 1
	 */
	void CThreadPoolModule::SetMaintenance(int iMaint)
	{
		try
		{
			m_iMaintenance = iMaint;
			if (m_iMaintenance == 0)
				CBaseTask::m_condMaintenance.notify_all();
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
}
