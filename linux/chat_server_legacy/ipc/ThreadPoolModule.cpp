
#include "ThreadPoolModule.h"
#include "ConfigurationModule.h"
//thirdparty
#include <fcntl.h>
#include <sys/socket.h>
#include <boost/bind.hpp>
#include <boost/thread/locks.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using namespace trc;

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	//maximum size of one data chunk to be read from network socket
	const int g_iMaxBufferSize = 512;
	//maximum number of data chunks we will handle from one user per one message
	const int g_iMaxChunksOfData = 8;
	//min size of the thread pool (min number of worker threads)
	const size_t g_iMinPoolSize = 2;
	//max size of the thread pool (max number of worker threads)
	const size_t g_iMaxPoolSize = 10;
	// delmiter for text parsing
	const std::string g_strServiseStrBegin = "$\\";
	const char g_strServiseStrEnd = '\r';

	using namespace boost::threadpool;

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded constructor for the socket wrapper class
	 * @param iSocketDesc - in, socket descriptor
	 */
	CBaseSocket::CBaseSocket(int iSocketDesc)
	{
		try
		{
			if (iSocketDesc < 0)
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
	 * Destructor for the socket wrapper class
	 */
	CBaseSocket::~CBaseSocket()
	{
		try
		{
			LOGDEBUG << "Erasing " << m_iSocketDescriptor;
			shutdown(m_iSocketDescriptor,SHUT_RDWR);
			close(m_iSocketDescriptor);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main routine responsible for reading user's data and re-sending it back to other connected clients
	 */
	void CReceiveTask::ReceiveData()
	{
		try
		{
			m_bIsTaskCompleted = false;
			char szBuffer[g_iMaxBufferSize] = {0};
			int iByteCount = g_iMaxBufferSize;

			//assume this client started transmitting data from scratch
			//let's summarize it block-by-block and parse into User and Service data.
			std::string strBuffer("");

			int index;
			for (index = 0; (index < g_iMaxChunksOfData) && (iByteCount == g_iMaxBufferSize); ++index)
			{
				if ( (iByteCount = recv(m_spSocket->GetDescriptor(),szBuffer,g_iMaxBufferSize,0)) == -1)
				{
					LOGERROR << "Error while reading from the socket, socketDesc="<<m_spSocket->GetDescriptor()<<", err="<<errno;
					break;
				}
				else
				{
					LOGDEBUG << "Bytes read: " << iByteCount << ". SocketD:"<<m_spSocket->GetDescriptor();
					strBuffer.append(szBuffer);
					if (iByteCount != g_iMaxBufferSize)
					{
						strBuffer.erase(index*g_iMaxBufferSize + iByteCount,strBuffer.length() - iByteCount + 1);
					}
					memset(szBuffer,0,sizeof(g_iMaxBufferSize));
				}
			}

			if ( (index == g_iMaxChunksOfData) && (iByteCount == g_iMaxBufferSize) )
			{
				//detect message overflow
				strBuffer.assign("\n --- Service message: Message limit reached ---\n");
				if (write(GetDescriptor(),strBuffer.c_str(),strBuffer.length()) == -1)
				{
					LOGERROR << "Error while sending 'limited data' message ("<<GetDescriptor()<<"), err="<<errno;
				}
			}
			else if (!strBuffer.empty())
			{
				//make copy of active sockets in order to perform send operation. No need to block parent thread
				//pool while we do resend
				tListActiveSockets listSockets;
				m_ptrThreadPool->GetConnectionList(listSockets);
				size_t npos = 0;

				std::list<std::string> strListOfMessages;
				npos = strBuffer.find(g_strServiseStrBegin);
				while (npos != std::string::npos)
				{
					strListOfMessages.push_back(strBuffer.substr(0,npos));
					strBuffer.erase(0,npos+g_strServiseStrBegin.length());
					npos = strBuffer.find(g_strServiseStrEnd);
					if (npos == std::string::npos)
						strBuffer.clear();
					else
						strBuffer.erase(0,npos+1);
					npos = strBuffer.find(g_strServiseStrBegin);
				}
				if (!strBuffer.empty())
					strListOfMessages.push_back(strBuffer);

				BOOST_FOREACH(int iSocket,listSockets)
				{
					if (GetDescriptor() != iSocket)
					{
						BOOST_FOREACH(const std::string& str, strListOfMessages)
						{
							if (write(iSocket,str.c_str(),str.length()) == -1)
							{
								LOGERROR << "Error while sending data to the remote socket ("<<iSocket<<"), err="<<errno;
							}
						}
					}
				}
			}

			m_bIsTaskCompleted = true;
			if (!m_bIsNotified)
			{
				m_bIsNotified = true;
				m_funcHandler(m_spSocket->GetDescriptor());
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Overloaded constructor for the Thread Pool Module. Requires non-zero thread pool size
	 * to initiate it. If incorrect size is specified, a error will be logged and data will be cured
	 * @param iPoolSize - in, pool size
	 */
	CThreadPoolModule::CThreadPoolModule(size_t iPoolSize)
	{
		try
		{
			if (iPoolSize < g_iMinPoolSize)
			{
				LOGERROR << "Invalid pool size was specified ("<<iPoolSize<<"), switch to minimum : "<<g_iMinPoolSize;
				iPoolSize = g_iMinPoolSize;
			}
			else if (iPoolSize > g_iMaxPoolSize)
			{
				LOGERROR << "Invalid pool size was specified ("<<iPoolSize<<"), switch to maximum : "<<g_iMaxPoolSize;
				iPoolSize = g_iMaxPoolSize;
			}

			m_spThreadPool.reset(new pool());
			m_spThreadPool->size_controller().resize(iPoolSize);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Adding task to the thread pool via assigning tasks to the thread queue
	 * @param spTask - in, reference to the wrapped task to be queued
	 */
	void CThreadPoolModule::AddTask(const spCReceiveTask& spTask)
	{
		try
		{
			boost::lock_guard<boost::mutex> lock(m_Access);
			spTask->AssignParrent(this);
			m_mapReceiveTasks[spTask->GetDescriptor()] = spTask;
			m_listActiveSockets.push_back(spTask->GetDescriptor());
			RenewTask(spTask);
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Rescheduling existing task by adding it to the thread queue
	 * @param spTask - in, reference to the wrapped task to be renewed
	 */
	void CThreadPoolModule::RenewTask(const spCReceiveTask& spTask)
	{
		try
		{
			m_spThreadPool->schedule(boost::bind(&CReceiveTask::ReceiveData,spTask));
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
	bool CThreadPoolModule::FindTaskBySocket(int iSocketFD, spCReceiveTask& spTarget)
	{
		try
		{
			boost::lock_guard<boost::mutex> lock(m_Access);
			tIterMapReceiveTasks iter = m_mapReceiveTasks.find(iSocketFD);
			if (iter != m_mapReceiveTasks.end())
			{
				if (iter->second->GetDescriptor() == iSocketFD)
				{
					spTarget = iter->second;
					if (spTarget->IsCompleted())
					{
						return true;
					}
					LOGDEBUG << "Task for socket <<"<<iSocketFD<<" not completed yet, skip reschedule";
				}
			}
		}
		CATCH
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Routine to find existing tasks based on the socket descriptor and delete it
	 * @param iSocketFD - in, socket file descriptor
	 */
	void CThreadPoolModule::RemoveTaskBySocket(int iSocketFD)
	{
		try
		{
			boost::lock_guard<boost::mutex> lock(m_Access);
			tIterMapReceiveTasks iter = m_mapReceiveTasks.find(iSocketFD);
			if (iter != m_mapReceiveTasks.end())
			{
				if (iter->second->GetDescriptor() == iSocketFD)
				{
					LOGDEBUG << "Erasing task for socket = " << iSocketFD;
					m_mapReceiveTasks.erase(iSocketFD);
				}
			}

			tIterListActiveSockets iterSock  = std::find(m_listActiveSockets.begin(),
								m_listActiveSockets.end(),iSocketFD);
			if (iterSock != m_listActiveSockets.end())
			{
				m_listActiveSockets.erase(iterSock);
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Get copy of the socket's list to some external container. Done to be useful for
	 * running tasks if they need to resend some data to another nodes. This will make the copy
	 * of list of sockets and return mutex so that consequent operations on this list are not blocked
	 * @param copy - in/out, reference to the container where  internal socket list will be copied to
	 */
	void CThreadPoolModule::GetConnectionList(tListActiveSockets& copy)
	{
		try
		{
			boost::lock_guard<boost::mutex> lock(m_Access);
			copy.resize(m_listActiveSockets.size());
			std::copy(m_listActiveSockets.begin(),m_listActiveSockets.end(),copy.begin());
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
}
