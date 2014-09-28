
// native
#include "IPCModule.h"
#include "Logger.h"
#include "ConfigurationModule.h"
// third-party
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <string>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	using namespace ipc;
	using namespace Config;

	CIPCModule* CIPCModule::m_pSelf = NULL;
	boost::mutex CIPCModule::m_ipcAccess;

	//timeout for pinging queue, in seconds
	const int g_iQueueTimeout = 5;
	//maximum number of events selector can handle frome existing sockets
	const int g_iMaxEvents = 500;
	//maximum number of queued connections on the main listening socket
	const int g_iMaxQueuedConnections = 5000;
	//assume we will survive when handling 4KB memory for each parameter
	const int g_iSegmentSize = 4096;

	//typedef special string for passing it between processes
	typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
	typedef basic_string<char, std::char_traits<char>, CharAllocator> IPCString;

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Default constructor
	 */
	CIPCModule::CIPCModule():
		m_iSelectorPipe{0,0},
		m_strTCPNetworkAddress(""),
		m_strUDPNetworkAddress(""),
		m_iTCPPort(0),
		m_iUDPPort(0),
		m_bIsFirstLaunch(false),
		m_ulSummaryOfParsing(0),
		m_ulSummaryAcceptedConn(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Public method to obtain singleton instance
	 * @return CIPCModule& - reference to the singleton object
	 */
	CIPCModule& CIPCModule::Instance()
	{
		try
		{
			if ( m_pSelf == NULL )
			{
				boost::lock_guard<boost::mutex> lock(m_ipcAccess);
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
			if (m_spThreadModule)
				LOGEMPTY << "\nDeadline statistics:"<<
				"\n\tsummary connections accepted:\t"<<m_ulSummaryAcceptedConn<<
				"\n\tparsing succeded:\t"<<m_ulSummaryOfParsing<<
				"\n\tpending-to-open sockets:\t"<<m_listPendingSockets.size()<<
				"\n\tpending-to-close sockets:\t"<<m_spThreadModule->GetNumberOfPendingSockets()<<
				"\n\tundeleted tasks:\t"<<m_spThreadModule->GetNumberOfTasks()<<std::endl;

			delete m_pSelf;
			m_pSelf = NULL;
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Tricky method to understand if previous process is still up and running. Create an IPC
	 * queue and post a message to it. Assuming some limited timeout
	 * should be enough for another process to read this message (if this 'another' process is alive). If it's not
	 * then we will still see this message in the queue - assume it as an answer on our question
	 * @return bool - true if the algorithm detected message in a queue which was not read by anyone. False if
	 *     message was successfully read by someone from this queue. The last case assumes we are not alone (the first)
	 */
	bool CIPCModule::IsFirstInstance()
	{
		try
		{
			try
			{
				//never specify custom deleter here - or the process will end and close the queue...
				m_spSharedMsgQueue.reset( new message_queue(open_only,SERVER_MSG_QUEUE));
			}
			catch(const ipc::interprocess_exception& ex)
			{
				m_bIsFirstLaunch = true;
				switch (ex.get_error_code())
				{
					case not_found_error:
						// exception is thrown when process cannot find a queue. Assume queue was not created
						// ever - so treat us as the only running instance
						return true;
					default:
						// bunch of exceptions can be received as well... Let's inform user and still try
						// (die hard ?) to create the queue at some step later
						LOGFATAL << "Exception while opening a shared queue, error code=" << ex.get_error_code()<<", message="<<ex.what();
						return true;
						break;
				}
			}

			size_t sizeOld = m_spSharedMsgQueue->get_num_msg();
			int iSignal = eCONFIG_UNDEFINED;
			{
				// need some TimedLocable IPC object to wait answer from remote process
				using namespace boost::posix_time;
				boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_MSG_QUEUE_MUTEX), boost::bind(&named_mutex::remove,SERVER_MSG_QUEUE_MUTEX));
				scoped_lock<named_mutex> lock(*spMutex.get());
				boost::shared_ptr<named_condition> spQueueCond(new named_condition(open_or_create,SERVER_MSG_QUEUE_COND), boost::bind(&named_condition::remove,SERVER_MSG_QUEUE_COND));
				ptime timeWake = boost::get_system_time() + seconds(g_iQueueTimeout);
				m_spSharedMsgQueue->send(&iSignal,sizeof(iSignal),1);
				spQueueCond->timed_wait(lock,timeWake);
			}

			size_t sizeNew = m_spSharedMsgQueue->get_num_msg();
			m_spSharedMsgQueue.reset();
			if (sizeNew <= sizeOld)
			{
				m_bIsFirstLaunch = false;
				return m_bIsFirstLaunch;
			}
		}
		catch(const ipc::interprocess_exception& ex)
		{
			LOGERROR << "Exception while working with a shared queue, error code=" << ex.get_error_code()<<", message="<<ex.what();
		}
		catch(...)
		{
			LOGERROR << "Exception while working with a shared queue";
		}
		//Eather our mechanism worked properly or some exception was raised - consider both cases as 'first launch'
		m_bIsFirstLaunch = true;
		return m_bIsFirstLaunch;
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Create the named IPC queue to exchange messages between processes
	 * This code should be invoked from the first instance of process only because message queue time-life should be controlled
	 * by one process only and it should be removed from the system with the last instance of process only
	 */
	void CIPCModule::CreateMessageQueue()
	{
		try
		{
			m_spSharedMsgQueue.reset( new message_queue(open_or_create,SERVER_MSG_QUEUE,100,sizeof(int)), boost::bind(&message_queue::remove,SERVER_MSG_QUEUE));
			m_spMemorySegment.reset(new managed_shared_memory(open_or_create,SERVER_SHARED_MEMORY,eCONFIG_COUNT*g_iSegmentSize), boost::bind(&shared_memory_object::remove,SERVER_SHARED_MEMORY));
			//launch reader thread only during the first launch
			boost::thread threadReader(boost::bind(&CIPCModule::SharedQueueReader,this));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Function to create internal thread pool with the parameters passed in
	 * @param iMaxNumberOfThreads - in, size of the pool we wish to create (maximum number of threads)
	 * @param strDataRegisterFile - in, reference to the string with data file with user credentials
	 * @param iSendTimeout - in, specify timeout the server should keep before sending the message back to client
	 */
	void CIPCModule::SetupThreadPool(size_t iMaxNumberOfThreads, const std::string& strDataRegisterFile, int iSendTimeout)
	{
		try
		{
			LOGDEBUG << "Setup IPC pool, size="<<iMaxNumberOfThreads<<", dataRegister="<<strDataRegisterFile;
			m_spThreadModule.reset(new CThreadPoolModule(iMaxNumberOfThreads,strDataRegisterFile,iSendTimeout));
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Propagate IP settings to IPC module from the configuration module storage. Should be called from main
	 * application thread only once
	 */
	void CIPCModule::SetupIPSettings()
	{
		try
		{
			CConfigurationModule& config = CConfigurationModule::Instance();
			std::string strTCPNetworkInterface("");
			std::string strUDPNetworkInterface("");
			config.GetSetting(eCONFIG_TCP_IF,strTCPNetworkInterface);
			config.GetSetting(eCONFIG_TCP_PORT,m_iTCPPort);
			config.GetSetting(eCONFIG_UDP_IF,strUDPNetworkInterface);
			config.GetSetting(eCONFIG_UDP_PORT,m_iUDPPort);

			//no dots in the IP address, assume it's a network adaptor name - let's try reading it
			//and converting to appropriate IPv4 address
			if (!strTCPNetworkInterface.empty() && strTCPNetworkInterface.find('.') == std::string::npos)
				GetIPAddressFromLocalAdaptor(strTCPNetworkInterface,m_strTCPNetworkAddress);

			if (m_strTCPNetworkAddress.empty())
			{
				//give up healing this junk, just assign back a config-read parameter and see if listeners
				//will detect failure and die
				m_strTCPNetworkAddress = strTCPNetworkInterface;
			}

			if (!strUDPNetworkInterface.empty() && strUDPNetworkInterface.find('.') == std::string::npos)
					GetIPAddressFromLocalAdaptor(strUDPNetworkInterface,m_strUDPNetworkAddress);

			if (m_strUDPNetworkAddress.empty())
			{
				m_strUDPNetworkAddress = strUDPNetworkInterface;
			}

			LOGDEBUG << "Will be working with network settings: TCP("<<m_strTCPNetworkAddress<<","<< m_iTCPPort<<"), UDP("<<m_strUDPNetworkAddress<<","<< m_iUDPPort<<")";
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main TCP-oriented listening routine. The aim is to open connection to dedicated network interface/port
	 * It works with a single socket only (create/bind and listen, synchronous I/O).
	 * Should any new client attempts to connect to it - thread will wake up, process
	 * connection and binds it to the ThreadModule pool. Any further activity of the connected
	 * client will be handled by TCPSelector thread
	 */
	void CIPCModule::StartTCPListener()
	{
		try
		{
			LOGDEBUG << "Start TCP listening";
			CBaseSocket socketBase;

			//package it inside a local socket so that we could get it destroyed in case of exception
			if ((socketBase = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
			{
				LOGFATAL << "Socket creating failed, err="<<errno;
				return;
			}

			struct sockaddr_in stSockAddr;
			memset(&stSockAddr, 0, sizeof(stSockAddr));
			stSockAddr.sin_family = PF_INET;
			stSockAddr.sin_port = htons(m_iTCPPort);
			in_addr_t addr = inet_addr(m_strTCPNetworkAddress.c_str());
			if (addr == INADDR_NONE)
			{
				CConfigurationModule& config = CConfigurationModule::Instance();
				std::string strAddr;
				config.GetDefaultValue<std::string>(eCONFIG_TCP_IF,strAddr);
				LOGERROR << "Internet address ("<<m_strTCPNetworkAddress<<") is invalid, try switching to default value ("<<strAddr<<")";
				if ( (addr = inet_addr(strAddr.c_str())) == INADDR_NONE)
				{
					LOGFATAL << "Unable to switch to default value";
					return;
				}
			}
			stSockAddr.sin_addr.s_addr = addr;
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
			socklen_t iSize = sizeof(stRemoteAddr);

			//main listening loop with blocking call to 'accept' method - it will not awake unless new
			//connection is attempted to be made
			tFuncSelectorNotifier func = boost::bind(&CIPCModule::NotifySelector,this,_1);
			while(true)
			{
				int ConnectFD = accept(socketBase, (sockaddr*)&stRemoteAddr, &iSize);
				if (ConnectFD == -1)
				{
					if (errno == EMFILE)
					{
						LOGERROR << "Too many opened connections";
					}
					else
					{
						LOGERROR << "Socket accept failed, err = "<<errno;
					}
					continue;
				}

				++m_ulSummaryAcceptedConn;
				spCTCPReceiveTask task(new CTCPReceiveTask(ConnectFD));
				task->AssignSelectorNotifier(func);
				m_spThreadModule->AddTask(task);
			}
		}
		CATCH
		LOGDEBUG << "Exit TCP listener thread";
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Main UDP-oriented listening routine. The aim is to open connection to dedicated network interface/port
	 * Should any new data appears on the socket the thread will wake up and pass information to separate
	 * thread in thread pool
	 */
	void CIPCModule::StartUDPListener()
	{
		try
		{
			LOGDEBUG << "Start UDP listening";
			CBaseSocket baseUDPSocket;
			//package socket inside a class so that we could get it closed in case of exception/return
			if ((baseUDPSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
			{
				LOGFATAL << "Socket creating failed, err="<<errno;
				return;
			}

			struct sockaddr_in stSockAddr;
			memset(&stSockAddr, 0, sizeof(stSockAddr));
			stSockAddr.sin_family = PF_INET;
			stSockAddr.sin_port = htons(m_iUDPPort);
			in_addr_t addr = inet_addr(m_strUDPNetworkAddress.c_str());
			if (addr == INADDR_NONE)
			{
				CConfigurationModule& config = CConfigurationModule::Instance();
				std::string strAddr;
				config.GetDefaultValue<std::string>(eCONFIG_UDP_IF,strAddr);
				LOGERROR << "Internet address ("<<m_strUDPNetworkAddress<<") is invalid, try switching to default value ("<<strAddr<<")";
				if ( (addr = inet_addr(strAddr.c_str())) == INADDR_NONE)
				{
					LOGFATAL << "Unable to switch to default value, err="<<errno;
					return;
				}
			}

			stSockAddr.sin_addr.s_addr = addr;
			if (bind(baseUDPSocket,(sockaddr *)&stSockAddr, sizeof(stSockAddr)) == -1)
			{
				LOGFATAL << "Socket bind failed, err="<<errno;
				return;
			}

			int iBytesRead = 0;
			char szBuffer[g_iMaxBufferSize] = {0};
			struct sockaddr_in sockRemote;
			memset(&sockRemote,0,sizeof(sockRemote));
			socklen_t addrLen = sizeof(sockRemote);
			std::string strBuffer;
			while(true)
			{
				if ( (iBytesRead = recvfrom(baseUDPSocket,szBuffer,g_iMaxBufferSize,0,
						(sockaddr*)&sockRemote,&addrLen)) < 0 )
				{
					if (errno == EMFILE)
					{
						LOGERROR << "Too many opened connections";
					}
					else
					{
						LOGERROR << "Socket accept failed, err = "<<errno;
					}
					continue;
				}
				else if (iBytesRead == 0)
				{

				}
				else
				{
					strBuffer.assign(szBuffer);
					m_spThreadModule->AddTask(baseUDPSocket,sockRemote,strBuffer);
				}
			}
		}
		CATCH
		LOGDEBUG << "Exit UDP listener thread";
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Selector is designed as a TCP-helper thread, very similar to TCP listener thread. The only and
	 * the main difference is that listener thread is working with one socket only and handles
	 * incoming connections only. Meanwhile selector has a list of already opened sockets
	 * (previously opened by listener) and works with them.
	 * List of active/monitoring sockets is passed to the epoll_wait function which will wake up every time
	 * any socket in a list has data to read. This function is used with the Level Triggered event
	 * (Edge Trigger is not specified therefore LT is used by default) which means that epoll_wait will
	 * wake up infinite number of times unless all sockets have no data to read. Once socket has any data to read
	 * - it is deleted from the list of monitored sockets, data is processed in a separate thread and then
	 * socket is returned back to the epoll_wait list of monitored sockets.
	 */
	void CIPCModule::StartTCPSelector()
	{
		try
		{
			LOGDEBUG << "Start Selector thread";
			int epollFD;
			struct epoll_event event;
			boost::scoped_array<epoll_event> arrEvents(new epoll_event[g_iMaxEvents]);

			epollFD = epoll_create1(0);
			if (epollFD == -1)
			{
				LOGFATAL << "Unable to setup selector, err=" <<errno;
				return;
			}

			if (pipe(m_iSelectorPipe) == -1)
			{
				LOGFATAL << "Unable to create signaling self-pipe, err=" <<errno;
				return;
			}

			//add reading end of pipe to controlling events
			event.data.fd = m_iSelectorPipe[0];
			event.events = EPOLLIN;
			if (epoll_ctl(epollFD, EPOLL_CTL_ADD, m_iSelectorPipe[0], &event) == -1)
			{
				LOGFATAL << "Unable to add descriptor controller, err=" <<errno;
				return;
			}

			int iNumDescriptors;
			while (true)
			{
				iNumDescriptors = epoll_wait(epollFD, arrEvents.get(), g_iMaxEvents, -1);

				//came from wait - assume some socket in a list of descriptors triggered
				//any of waited events
				m_ulSummaryOfParsing += m_spThreadModule->RemovePendingTasks();

				//No go through all triggered socket descriptors and find out what's going on with each of them
				//Three main cases are reviewed below:
				// a. Error occurred against a descriptor
				// b. Descriptor is a self-pipe so we need to add one new socket to the monitoring list
				// c. Descriptor is an active socket with some data to read - need to remove it from monitoring list
				//    and process data in a separate thread
				for (int i = 0; i < iNumDescriptors; ++i)
				{
					// Case a. (error)
					if ((arrEvents[i].events & EPOLLERR) ||
						(arrEvents[i].events & EPOLLHUP) ||
						(!(arrEvents[i].events & EPOLLIN))	)
					{
						// an error has occured on this fd, or the socket is not ready for reading
						LOGWARN << "Error in epoll_wait, err = "<<errno<<". Forse closing desc:"<<arrEvents[i].data.fd;
						unsigned long ulTemp = 0;

						if (epoll_ctl(epollFD, EPOLL_CTL_DEL, arrEvents[i].data.fd, &event) == -1)
						{
							LOGERROR << "Unable to remove descriptor ("<<arrEvents[i].data.fd<<") controller #1, err="<<errno;
						}
						m_spThreadModule->RemoveTaskBySocket(arrEvents[i].data.fd,ulTemp);
						continue;
					}
					// Case b. (self-pipe, new socket arrived)
					else if (m_iSelectorPipe[0] == arrEvents[i].data.fd)
					{
						LOGDEBUG << "Pipe signal, handle new socket";

						int value;
						if (read(m_iSelectorPipe[0],&value,sizeof(value)) == -1)
						{
							LOGERROR << "Error while reading from pipe ("<<m_iSelectorPipe[0]<<"), err= "<<errno;
							continue;
						}

						//less than zero means socket is closed - simply skip it
						if (value < 0)
							continue;

						{
							// lock array with new pending sockets
							boost::lock_guard<boost::mutex> lock(m_localAccess);
							BOOST_FOREACH(int iSocketFD, m_listPendingSockets)
							{
								LOGDEBUG << "Process socketFD: "<<iSocketFD;
								event.data.fd = iSocketFD;
								event.events = EPOLLIN;
								if (epoll_ctl(epollFD, EPOLL_CTL_ADD, iSocketFD, &event) == -1)
								{
									LOGERROR << "Unable to set descriptor controller, err = " <<errno;
									continue;
								}
							}
							m_listPendingSockets.clear();
						}
					}
					// Case c. (new data is available in one of the opened active sockets)
					else
					{
						LOGDEBUG << "Process data from existing connection, socketFD:"<<arrEvents[i].data.fd;
						spCTCPReceiveTask spTask;
						if (m_spThreadModule->FindTaskBySocket(arrEvents[i].data.fd,spTask))
						{
							if (epoll_ctl(epollFD, EPOLL_CTL_DEL, arrEvents[i].data.fd, &event) == -1)
							{
								LOGERROR << "Unable to remove descriptor ("<<arrEvents[i].data.fd<<") controller #2, err="<<errno;
							}
							m_spThreadModule->RenewTask(spTask);
						}
					}
				} //for (... signalled descriptors ...)
			}//while (!m_bExitListenThreads)
		}
		CATCH
		LOGDEBUG << "Exit TCP selector thread";
	}

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Consider command line options having more priority for the process. So traverse through known
	 * options and put them in the shared memory so that another process could read them and accept
	 * immediately. This method should be invoked from second instance of process only as its main role
	 * is to send data to remote process
	 */
	void CIPCModule::ApplyServerOptionsRemotely()
	{
		//some guard rail as per comment above
		if (m_bIsFirstLaunch)
			return;

		try
		{
			int iValue = 0;
			std::string strValue = "";
			using namespace Config;
			CConfigurationModule& config = CConfigurationModule::Instance();
			m_spMemorySegment.reset(new managed_shared_memory(open_only,SERVER_SHARED_MEMORY));
			LOGDEBUG << "Applying server options (remotely)";
			for (int iParamIndex = eCONFIG_UDP_PORT+1; iParamIndex < eCONFIG_COUNT; ++iParamIndex)
			{
				if (!config.IsParameterPresent(iParamIndex))
					continue;

				switch (iParamIndex)
				{
					case eCONFIG_KILL_PROCESS:
					case eCONFIG_DAEMON_MODE:
						//this params are to be skipped, not intended for sending this out
						break;
					case eCONFIG_DATA_FILE:
					{
						iValue = iParamIndex;
						if (config.GetProgramOption(iParamIndex,strValue))
						{
							boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_MSG_QUEUE_MUTEX));
							scoped_lock<named_mutex> lock(*spMutex.get());
							std::string strParamName = config.GetParameterName(eCONFIG_DATA_FILE);
							if (m_spMemorySegment->find<IPCString>(strParamName.c_str()).first)
							{
								LOGDEBUG << "Destroy memory object: " << strParamName;
								m_spMemorySegment->destroy<IPCString>(strParamName.c_str());

							}
							m_spMemorySegment->construct<IPCString>(strParamName.c_str())(strValue.c_str(),m_spMemorySegment->get_segment_manager());
							message_queue mqueue(open_only,SERVER_MSG_QUEUE);
							mqueue.send(&iValue,sizeof(iValue),1);
						}
						break;
					}
					case eCONFIG_MAINT:
					case eCONFIG_SLEEP:
					case eCONFIG_LOG_LEVEL:
					case eCONFIG_THREAD_POOL:
						if (config.GetProgramOption(iParamIndex,iValue))
						{
							boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_MSG_QUEUE_MUTEX));
							scoped_lock<named_mutex> lock(*spMutex.get());
							std::string strParamName = config.GetParameterName(iParamIndex);
							if (m_spMemorySegment->find<int>(strParamName.c_str()).first)
							{
								LOGDEBUG << "Destroy memory object: " << strParamName;
								m_spMemorySegment->destroy<int>(strParamName.c_str());
							}
							m_spMemorySegment->construct<int>(strParamName.c_str())(iValue);
							message_queue mqueue(open_only,SERVER_MSG_QUEUE);
							mqueue.send(&iParamIndex,sizeof(iParamIndex),1);
						}
						break;
					default:
						LOGERROR << "Trying to handle unknown parameter, id = " << iParamIndex;
				}
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Inter-thread method, used to notify TCPSelector thread about new incoming connections
	 * which were established by the Listener thread. Notification is performed via unnamed pipe.
	 * Write-end of the pipe belongs to the worker thread which was handling new incoming conn recently
	 * Read-end of the pipe belongs to the Selector thread which is constantly waiting for 'read'
	 * event via blocking call to the epoll_wait method
	 * @param iNewDescriptor - in, new descriptor to be propagated to the TCPSelector monitoring list
	 */
	void CIPCModule::NotifySelector(int iNewDescriptor)
	{
		try
		{
			LOGDEBUG << "Notify," << iNewDescriptor;
			if (iNewDescriptor > 0)
			{
				boost::lock_guard<boost::mutex> lock(m_localAccess);
				m_listPendingSockets.push_back(iNewDescriptor);
			}

			//check write-end of the pipe
			if (m_iSelectorPipe[1] == 0)
			{
				LOGFATAL << "Error while handling write-end of the pipe.";
				return;
			}

			int value = iNewDescriptor;
			if (write(m_iSelectorPipe[1],&value,sizeof(value)) == -1)
			{
				LOGERROR << "Error while writing to pipe ("<<m_iSelectorPipe[1]<<"), err= "<<errno;
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * Blocking method which wait for a message from the queue. Is launched in
	 * a separate thread. Mainly is used for IPC communication like accepting params from
	 * another process command line
	 */
	void CIPCModule::SharedQueueReader()
	{
		try
		{
			if (m_spSharedMsgQueue)
			{
				bool bNeedExit = false;
				int iReceiver = 0;
				size_t sizeReceived;
				unsigned int iPriority;
				CConfigurationModule& config = CConfigurationModule::Instance();
				while(!bNeedExit)
				{
					// insert try-catch here to handle exception inside a cycle and survive the thread
					try
					{
						LOGDEBUG << "In the reader loop";
						m_spSharedMsgQueue->receive(&iReceiver,sizeof(iReceiver),sizeReceived,iPriority);
						if (sizeof(iReceiver) != sizeReceived)
							LOGERROR << "Message size is incorrect: "<<sizeReceived<<", where ought to be:"<<sizeof(iReceiver);
						else
							LOGDEBUG << "Message received, size="<<sizeReceived<<", value="<<iReceiver;

						switch(iReceiver)
						{
							case eCONFIG_UNDEFINED:
							{
								//service message for another instance of server to check that we
								//are alive. Create an IPC object and send a notify response
								named_condition cond(open_or_create,SERVER_MSG_QUEUE_COND);
								//need this hack to avoid race condition when we got the signal and notify
								//condition even before other process attempted to start waiting for it
								boost::this_thread::sleep(boost::posix_time::milliseconds(100));
								cond.notify_all();
								break;
							}
							case eCONFIG_DATA_FILE:
							{
								boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_MSG_QUEUE_MUTEX),boost::bind(&named_mutex::remove,SERVER_MSG_QUEUE_MUTEX));
								scoped_lock<named_mutex> lock(*spMutex.get());
								std::string strParamName = config.GetParameterName(eCONFIG_DATA_FILE);
								IPCString* istrValue = m_spMemorySegment->find<IPCString>(strParamName.c_str()).first;
								if ( istrValue != NULL)
								{
									LOGDEBUG << "Received string IPC data: " << istrValue->c_str();
									std::string strTempValue(istrValue->c_str());
									config.SetProgramOption(iReceiver,strTempValue);
									m_spMemorySegment->destroy_ptr(istrValue);
								}
								break;
							}
							case eCONFIG_MAINT:
							case eCONFIG_SLEEP:
							case eCONFIG_LOG_LEVEL:
							case eCONFIG_THREAD_POOL:
							{
								boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_MSG_QUEUE_MUTEX),boost::bind(&named_mutex::remove,SERVER_MSG_QUEUE_MUTEX));
								scoped_lock<named_mutex> lock(*spMutex.get());
								std::string strParamName = config.GetParameterName(iReceiver);
								int* iPtrValue = m_spMemorySegment->find<int>(strParamName.c_str()).first;
								if ( iPtrValue != NULL)
								{
									LOGDEBUG << "Received integer IPC data: " << *iPtrValue;
									config.SetProgramOption(iReceiver,*iPtrValue);
									m_spMemorySegment->destroy_ptr(iPtrValue);
								}
								break;
							}
							case eCONFIG_KILL_PROCESS:
							{
								//Break the cycle and notify destructor for it to finish the work
								bNeedExit = true;

								//notify named_condition so that remote process could awake and terminate
								named_condition cond(open_or_create,SERVER_CLOSE_COND);
								cond.notify_all();
								break;
							}
							default:
								LOGERROR << "Unknown type of message received: "<<iReceiver;
						}
					}
					CATCH_IPC
					CATCH
				} //while(!bNeedExit)
			} //if (m_spSharedMsgQueue)
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
	/*
	 * There is a possibiilty that network adaptor will be specified by the name (like eth0) rather than
	 * then ip address. So let's find this out and convert it to IP if possible
	 * @param strNetworkInterface - in, ref to the name of the network interface to check for valid IP
	 * @param strIpAddress - in/out, string with the IP address if any is found, empty string otherwise
	 *
	 */
	void CIPCModule::GetIPAddressFromLocalAdaptor(const std::string& strNetworkInterface, std::string& strIpAddress)
	{
		strIpAddress.clear();
		try
		{
			struct ifreq ifr;
			CBaseSocket socketTemp;
			char szIPAddr[INET_ADDRSTRLEN+1] = {0};

			//create temp socket
			if( (socketTemp = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				LOGDEBUG << "Unable to create temporary socket, err="<<errno;
				return;
			}

			ifr.ifr_addr.sa_family = PF_INET;
			strncpy(ifr.ifr_name, strNetworkInterface.c_str(), IFNAMSIZ);
			//try reading the IP address
			if (ioctl(socketTemp, SIOCGIFADDR, &ifr) < 0)
			{
				LOGERROR << "Unable to read device information, err="<<errno;
				return;
			}
			else
			{
				if (inet_ntop(PF_INET, (const void*)&(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr),
						szIPAddr, INET_ADDRSTRLEN+1) == NULL)
				{
					LOGERROR << "Unable to convert network address, err="<<errno;
					return;
				}
				strIpAddress.assign(szIPAddr);
			}
		}
		CATCH
	}

	//////////////////////////////////////////////////////////////////////////
 } // namespace IPC
