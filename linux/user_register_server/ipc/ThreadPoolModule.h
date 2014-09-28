
#ifndef THREADPOOLMODULE_H_
#define THREADPOOLMODULE_H_

//native
#include "ProcessData.h"
//thirdparty
#include <list>
#include <map>
#include <fstream>
#include <netinet/in.h>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <threadpool/threadpool.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	//maximum size of one data chunk to be read from network socket
	//TODO: Better align this to MTU size to reduce read/data processing overhead
	const int g_iMaxBufferSize = 1600;

	//some forward typedefs
	class CBaseSocket;
	class CTCPReceiveTask;
	class CThreadPoolModule;
	typedef boost::shared_ptr<CTCPReceiveTask> spCTCPReceiveTask;
	typedef boost::shared_ptr<CBaseSocket> spCBaseSocket;
	typedef std::map<int,spCTCPReceiveTask> tMapReceiveTasks;
	typedef tMapReceiveTasks::iterator tIterMapReceiveTasks;
	typedef boost::function<void(int)> tFuncSelectorNotifier;
	typedef struct sockaddr_in tSockAddr;

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Auxiliary class to encapsulate socket descriptor and provide RAII-style closing
	 * for it once CBaseSocket goes out of scope
	 */
	class CBaseSocket
	{
	public:
		CBaseSocket():m_iSocketDescriptor(0){;}
		CBaseSocket(int iSocketDesc);
		virtual ~CBaseSocket();
		int GetDescriptor() {return m_iSocketDescriptor;}
		operator int() const {return m_iSocketDescriptor;}
		CBaseSocket& operator= (int iSocketFD)
		{
			m_iSocketDescriptor = iSocketFD;
			return *this;
		}

	private: //methods
		bool IsConnected();

	private: //members
		int m_iSocketDescriptor;
	};


	//////////////////////////////////////////////////////////////////////////
	/**
	 * Base class which performs similar data processing (parsing/composing the answer/
	 * responding to client) for both UDP and TCP clients
	 */
	class CBaseTask
	{
	private: //permit no copy and no explicit creation (socket desc is a requirement)
		CBaseTask();
		CBaseTask(const CBaseTask& src);
		CBaseTask& operator= (const CBaseTask& src);

	public: //permit no copy and implicit creation
		CBaseTask(int iSocketDesc):
			m_ptrThreadPool(NULL),
			m_bIsTaskCompleted(false),
			m_strMsgBuffer(""),
			m_strDataRegisterFile(""),
			m_ulSuccededParsing(0),
			m_iSocketDesc(iSocketDesc)
		{
			m_spProcessData.reset(new CProcessData());
		}

	public: //methods
		void ProcessData(bool bIsUDPSocket = false);
		int ReadFile(std::list<std::string>& listRecords);
		void AssignParrent(CThreadPoolModule* ptrParent) { m_ptrThreadPool = ptrParent; }
		int GetDescriptor() { return m_iSocketDesc; }
		bool IsCompleted() { return m_bIsTaskCompleted; }
		unsigned long GetParsingStatistics() {return m_ulSuccededParsing; }
		void AssignMessageBuffer(const std::string& strBuf) { m_strMsgBuffer = strBuf; }
		void AssignUDPInfo(const tSockAddr& udpClient) { m_udpClient = udpClient; }
		void SleepBeforeSend(const boost::posix_time::ptime& ptStart, int iTimeout);

	public: //members
		static boost::condition_variable m_condMaintenance;

	protected: //members
		static boost::shared_mutex m_sharedFileAccess;
		static boost::mutex m_mutexMaintenance;
		CThreadPoolModule* m_ptrThreadPool;
		boost::shared_ptr<CProcessData> m_spProcessData;
		bool m_bIsTaskCompleted;
		std::string m_strMsgBuffer;
		std::string m_strDataRegisterFile;
		unsigned long m_ulSuccededParsing;
		int m_iSocketDesc;
		tSockAddr m_udpClient;
	};

	//////////////////////////////////////////////////////////////////////////
	/**
	 * Class that has some TCP-specific routines like receiving & summarizing data
	 * from a client, marking task for deletion if remote client is closed. TCP clients are handled
	 * within this kind of task as opposite to UDP clients which can be handled within CBaseTask
	 */
	class CTCPReceiveTask: public CBaseTask
	{
	private: //permit no copy and default creation
		CTCPReceiveTask();
		CTCPReceiveTask(const CTCPReceiveTask& src);
		CTCPReceiveTask& operator= (const CTCPReceiveTask& src);

	public:
		CTCPReceiveTask(int iSocketDesc):
			CBaseTask(iSocketDesc),
			m_bPendingDelete(false),
			m_bIsFirstTime(false)
		{
			m_spSocket.reset(new CBaseSocket(iSocketDesc));
		}

		void ReceiveData();
		void AssignSelectorNotifier(const tFuncSelectorNotifier& func) {m_funcHandler = func;}
		int GetDescriptor() { return m_spSocket->GetDescriptor(); }

	private:
		spCBaseSocket m_spSocket;
		bool m_bPendingDelete;
		bool m_bIsFirstTime;
		tFuncSelectorNotifier m_funcHandler;
	};


	//////////////////////////////////////////////////////////////////////////
	/**
	 * Core class to provide thread pool management to the IPC Module. It uses external component
	 * to maintain internal thread pool.
	 */
	class CThreadPoolModule
	{
	private: //permit no copy and default creation
		CThreadPoolModule();
		CThreadPoolModule(const CThreadPoolModule& src);
		CThreadPoolModule& operator= (const CThreadPoolModule& src);

	public: //methods
		CThreadPoolModule(size_t iPoolSize, const std::string& strDataRegisterFile, int iSendTimeout);
		void AddTask(const spCTCPReceiveTask& spTask);
		void AddTask(int iUDPSocket, const tSockAddr& udpClient, const std::string& strBuffer);
		void RenewTask(const spCTCPReceiveTask& spTask);
		void ProcessUDPData(int iUDPSocket, tSockAddr udpClient, std::string strBuffer);
		bool FindTaskBySocket(int iSocketFD, spCTCPReceiveTask& spTarget);
		bool RemoveTaskBySocket(int iSocketFD, unsigned long& ulSuccededParsing);
		void AddPendingRemove(int iSocketFD);
		unsigned long RemovePendingTasks();
		std::string GetResponseMessage(int iResponseCode);
		unsigned long GetNumberOfTasks() { return m_mapReceiveTasks.size(); }
		unsigned long GetNumberOfPendingSockets() { return m_listPendingRemoveSockets.size(); }

		//methods to propagate config settings in run-time
		void SetSendTimeout(int iSendTimeout) { m_iSendTimeout = iSendTimeout; }
		int GetSendTimeout() { return m_iSendTimeout; }
		void SetDataPath(const std::string& strDataPath) { m_strDataRegisterFile = strDataPath; }
		std::string GetDataPath() { return m_strDataRegisterFile; }
		void SetMaintenance(int iMaint);
		int GetMaintenance() { return m_iMaintenance; }

	private: //members
		tMapReceiveTasks m_mapReceiveTasks;
		std::list<int> m_listPendingRemoveSockets;
		std::map<int,std::string> m_mapResponseMessages;
		std::string m_strDataRegisterFile;
		int m_iSendTimeout;
		int m_iMaintenance;
		boost::shared_ptr<boost::threadpool::pool> m_spThreadPool;
		boost::shared_mutex m_sharedTaskAccess;
		boost::recursive_mutex m_RemoveAccess;
	};

	//////////////////////////////////////////////////////////////////////////
} //namespace IPC
#endif /* THREADPOOLMODULE_H_ */
