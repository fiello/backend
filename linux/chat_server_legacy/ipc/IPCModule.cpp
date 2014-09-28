
// native
#include "IPCModule.h"
#include "Logger.h"
#include "CompiledDefinitions.h"
#include "ConfigurationModule.h"
// third-party
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	using namespace ipc;

	CIPCModule* CIPCModule::m_pSelf = NULL;
	ipc::interprocess_mutex CIPCModule::m_ipcAccess;

	//timeout for pinging queue, in seconds
	const static int g_iQueueTimeout = 5;
	//maximum number of events selector can handle frome existing sockets
	const static int g_iMaxEvents = 500;
	//maximum number of queued connections on the main listening socket
	const static int g_iMaxQueuedConnections = 5000;

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Default constructor
	 */
	CIPCModule::CIPCModule():
		m_strNetworkInterface(""),
		m_iPort(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Public method to obtain singleton instance
	 */
	CIPCModule& CIPCModule::Instance()
	{
		try
		{
			if ( m_pSelf == NULL )
			{
				scoped_lock<interprocess_mutex> lock(m_ipcAccess);
				if ( m_pSelf == NULL )
				{
					m_pSelf = new CIPCModule();
				}
			}
		}
		CATCH
		return *m_pSelf;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Method to destroy IPC singleton explicitly
	 */
	void CIPCModule::Destroy()
	{
		try
		{
			delete m_pSelf;
			m_pSelf = NULL;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Tricky method to understand if previous process is still up and running. Create an IPC
	 * queue and post a message there. Assuming a separate thread for it some limited timeout
	 * should be enough for another process to read this message - if the process is alive. If it's not
	 * then we will still see this message in the queue - assusme it as an answer on our question
	 */
	bool CIPCModule::IsFirstInstance()
	{
		try
		{
			try
			{
				scoped_lock<interprocess_mutex> lock(m_ipcAccess);
				m_spSharedMsgQueue.reset( new message_queue(open_only,SERVER_QUEUE) );
			}
			catch(const ipc::interprocess_exception& ex)
			{
				LOGDEBUG << "Shared queue is not in place";
				return true;
			}
			size_t sizeOld = m_spSharedMsgQueue->get_num_msg();
			int iSignal = 1;
			m_spSharedMsgQueue->send(&iSignal,sizeof(int),1);
			LOGDEBUG << "Detecting (timeout="<<g_iQueueTimeout<<" sec) old process functionality";
			sleep(g_iQueueTimeout);
			size_t sizeNew = m_spSharedMsgQueue->get_num_msg();
			if (sizeNew > sizeOld)
			{
				LOGWARN << "Queue is ready for the new process";
				return true;
			}

			LOGDEBUG << "Another process seems to function properly.";
		}
		CATCH
		//in case of any issue let's treat this as not first launch since relying on 'already'
		//started process is not robust
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Create an IPC queue to move messages between processes
	 */
	void CIPCModule::CreateMessageQueue()
	{
		try
		{
			{
				scoped_lock<interprocess_mutex> lock(m_ipcAccess);
				m_spSharedMsgQueue.reset( new message_queue(open_or_create,SERVER_QUEUE,100,sizeof(int)), boost::bind(&message_queue::remove,SERVER_QUEUE));
			}

			boost::thread threadReader(boost::bind(&CIPCModule::SharedQueueReader,this));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Blocking method which wait for a message from the queue. Is launched in
	 * a separate thread
	 */
	void CIPCModule::SharedQueueReader()
	{
		try
		{
			LOGDEBUG << "Queue reader started";
			if (m_spSharedMsgQueue)
			{
				while(true)
				{
					LOGDEBUG << "Inside a loop";
					int iReceiver = 0;
					size_t sizeReceived;
					unsigned int iPriority;
					m_spSharedMsgQueue->receive(&iReceiver,sizeof(int),sizeReceived,iPriority);
					if (sizeof(iReceiver) != sizeReceived)
						LOGERROR << "Unknown message received, skip it";
					else
						LOGDEBUG << "Message received, size="<<sizeReceived<<", value="<<iReceiver;
				}
			}
		}
		CATCH_IPC
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main listening routine. The aim is to open connection to dedicated network device
	 * It works with a single socket only (create/bind and listen, syncronous I/O).
	 * Should any new client attempts to connect to it - thread will wake up, process
	 * connection and binds it to the ThreadModule pool.
	 */
	void CIPCModule::StartListener(size_t iNumberOfThreads)
	{
		try
		{
			LOGDEBUG << "Start listening.";
			m_spThreadModule.reset(new CThreadPoolModule(iNumberOfThreads));

			struct sockaddr_in stSockAddr;
			int SocketFD;

			if ((SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
			{
				LOGERROR << "Socket creating faield, err="<<errno;
				return;
			}
			//package it inside a local socket so that we could get it destroyed in case of exc.
			CBaseSocket socketBase(SocketFD);

			memset(&stSockAddr, 0, sizeof(stSockAddr));
			stSockAddr.sin_family = AF_INET;
			stSockAddr.sin_port = htons(m_iPort);
			stSockAddr.sin_addr.s_addr = inet_addr(m_strNetworkInterface.c_str());

			if (bind(socketBase,(sockaddr *)&stSockAddr, sizeof(stSockAddr)) == -1)
			{
				LOGERROR << "Socket bind failed, err="<<errno;
				return;
			}

			if (listen(socketBase, g_iMaxQueuedConnections) == -1)
			{
				LOGERROR << "Socket listen failed, err="<<errno;
				return;
			}

			struct sockaddr_in stRemoteAddr;
			socklen_t iSize = 0;
			iSize = sizeof(sockaddr_in);

			//main listening loop with blocking call to 'accept' method - it will not awake unless new
			//connection is attempted to be made
		    for(;;)
		    {
		    	int ConnectFD = accept(socketBase, (sockaddr*)&stRemoteAddr, &iSize);
				if (ConnectFD == -1)
				{
					LOGERROR << "Socket accept failed, err = "<<errno;
					return;
				}

				LOGDEBUG << "Accepted connection from "<<ntohs(stRemoteAddr.sin_port);
				spCReceiveTask task;
				task.reset(new CReceiveTask(ConnectFD));
				tFuncSelectorNotifier func = boost::bind(&CIPCModule::NotifySelector,this,_1);
				task->AssignSelectorNotifier(func);
				m_spThreadModule->AddTask(task);
		    }
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Selector is designed as a second thread, very similar to listener thread. The only and
	 * the main difference is that listener thread is working with one socket only and handles
	 * incoming connections only. Meanwhile selector has a list of already opened sockets
	 * (previously opened by listener) and works with them. Also it performs write operations when
	 * we need to respond to the client
	 */
	void CIPCModule::StartSelector()
	{
		try
		{
			int epollFD;
			struct epoll_event event;
			boost::scoped_array<epoll_event> arrEvents(new epoll_event[g_iMaxEvents]);

			epollFD = epoll_create1 (0);
			if (epollFD == -1)
			{
				LOGFATAL << "Unable to launch data selector, err = " <<errno;
				return;
			}

			if (pipe(m_iPipeFDs) == -1)
			{
				LOGFATAL << "Unable to create signaling self-pipe, err" <<errno;
				return;
			}

			//add reading end of pipe to controlling events
			event.data.fd = m_iPipeFDs[0];
			event.events = EPOLLIN | EPOLLET;
			if (epoll_ctl(epollFD, EPOLL_CTL_ADD, m_iPipeFDs[0], &event) == -1)
			{
				LOGERROR << "Unable to set descriptor controller, err = " <<errno;
				return;
			}

			bool bExit = false;
			int iNumDescriptors;
			while (!bExit)
			{
				iNumDescriptors = epoll_wait(epollFD, arrEvents.get(), g_iMaxEvents, -1);
				for (int i = 0; i < iNumDescriptors; ++i)
				{
					if ((arrEvents[i].events & EPOLLERR) ||
						(arrEvents[i].events & EPOLLHUP) ||
						(!(arrEvents[i].events & EPOLLIN))	)
					{
						// an error has occured on this fd, or the socket is not ready for reading
						LOGWARN << "Error in epoll_wait, err = "<<errno<<". Forse closing desc:"<<arrEvents[i].data.fd;

						m_spThreadModule->RemoveTaskBySocket(arrEvents[i].data.fd);
						continue;
					}
					else if (m_iPipeFDs[0] == arrEvents[i].data.fd)
					{
						LOGDEBUG << "Pipe signal, let's handle new socket";

						uint64_t value;
						if (read(m_iPipeFDs[0],&value,sizeof(value)) == -1)
						{
							LOGERROR << "Error while reading from pipe ("<<m_iPipeFDs[0]<<"), err= "<<errno;
						}

						// lock array with new pending sockets
						{
							scoped_lock<interprocess_mutex> lock(m_ipcAccess);
							BOOST_FOREACH(int iSocketFD, m_listPendingSock)
							{
								LOGDEBUG << "Process socketFD: "<<iSocketFD;
								event.data.fd = iSocketFD;
								event.events = EPOLLIN | EPOLLET;
								if (epoll_ctl(epollFD, EPOLL_CTL_ADD, iSocketFD, &event) == -1)
								{
									LOGERROR << "Unable to set descriptor controller, err = " <<errno;
									continue;
								}
							}
							m_listPendingSock.clear();
						}
					}
					else
					{
						LOGDEBUG << "Process data from existing connection, socketFD:"<<arrEvents[i].data.fd;
						spCReceiveTask spTask;
						if (m_spThreadModule->FindTaskBySocket(arrEvents[i].data.fd,spTask))
						{
							m_spThreadModule->RenewTask(spTask);
						}
					}
				} //for (... signalled descriptors ...)
			}//while (!bExit)
		}
		CATCH
	}


	//////////////////////////////////////////////////////////////////////////
	/*
	 * Propagate IP settings to IPC module from the main application thread
	 * @param strNetworkInterface - in, local network interface to bind connection to
	 * @param iPort - in, local port
	 */
	void CIPCModule::SetIPSettings(const std::string& strNetworkInterface, int iPort)
	{
		try
		{
			if (strNetworkInterface.empty())
				m_strNetworkInterface = "";
			else
				m_strNetworkInterface = strNetworkInterface;

			//consider some bounds for possible ports
			if (iPort <= 0 && iPort > 65000)
				m_iPort = 1024;
			else
				m_iPort = iPort;

			LOGDEBUG << "Will be working with network settings ("<<m_strNetworkInterface<<","<< m_iPort<<")";
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Inter-thread method, used to notify Selector thread about new incoming connections
	 * which were established by the Listener thread. Notification is performed via unnamed pipe.
	 * Write-end of the pipe belongs to the worker thread which was handling new incoming conn recenly
	 * Read-end of the pipe belongs to the Selector thread which is constantly waiting for 'read'
	 * event via blocking call to the epoll_wait method
	 */
	void CIPCModule::NotifySelector(int iNewDescriptor)
	{
		try
		{
			LOGDEBUG << "Notify," << iNewDescriptor;
			{
				scoped_lock<interprocess_mutex> lock(m_ipcAccess);
				m_listPendingSock.push_back(iNewDescriptor);
			}

			//check write-end of the pipe
			if (m_iPipeFDs[1] == 0)
			{
				LOGFATAL << "Error while handling existing connection";
				return;
			}

			uint64_t value = 0;
			if (write(m_iPipeFDs[1],&value,sizeof(value)) == -1)
			{
				LOGERROR << "Error while writing to pipe ("<<m_iPipeFDs[1]<<"), err= "<<errno;
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
 } // namespace IPC
