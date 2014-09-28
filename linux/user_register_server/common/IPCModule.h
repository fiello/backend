
#ifndef IPCMODULE_H_
#define IPCMODULE_H_

//native
#include "ThreadPoolModule.h"
//thirdparty
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <list>

//////////////////////////////////////////////////////////////////////////
namespace IPC
{
	namespace ipc = boost::interprocess;

	//////////////////////////////////////////////////////////////////////
	/*
	 * IPC module, designed as a singleton and intended to handle all IPC routines, that is:
	 * creating thread pool, managing/scheduling tasks, performing synchronous I/O in sockets/pipes
	 */
	class CIPCModule
	{
	private: // permit no copy, default ctors/dtors
		CIPCModule();
		CIPCModule(const CIPCModule& src);
		CIPCModule& operator= (const CIPCModule& src);
		~CIPCModule(){;}

	public: // methods
		static CIPCModule& Instance();
		void Destroy();
		bool IsFirstInstance();
		void CreateMessageQueue();
		void SetupThreadPool(size_t iMaxNumberOfThreads, const std::string& strDataRegisterFile, int iSendTimeout);
		void SetupIPSettings();
		void StartTCPListener();
		void StartTCPSelector();
		void StartUDPListener();
		void ApplyServerOptionsRemotely();
		void SetSendTimeout(int iSendTimeout) { m_spThreadModule->SetSendTimeout(iSendTimeout); }
		void SetDataPath(const std::string& strDataPath) { m_spThreadModule->SetDataPath(strDataPath); }
		void SetMaintenanceMode(int iMode) { m_spThreadModule->SetMaintenance(iMode); }

	private: //members
		static CIPCModule* m_pSelf;
		static boost::mutex m_ipcAccess;
		boost::mutex m_localAccess;
		boost::shared_ptr<ipc::message_queue> m_spSharedMsgQueue;
		boost::shared_ptr<CThreadPoolModule> m_spThreadModule;
		boost::shared_ptr<ipc::managed_shared_memory> m_spMemorySegment;
		std::list<int> m_listPendingSockets;
		int m_iSelectorPipe[2];

		//IP settings
		std::string m_strTCPNetworkAddress;
		std::string m_strUDPNetworkAddress;
		int m_iTCPPort;
		int m_iUDPPort;
		bool m_bIsFirstLaunch;
		unsigned long m_ulSummaryOfParsing;
		unsigned long m_ulSummaryAcceptedConn;

	private: //methods
		void NotifySelector(int iNewDescriptor);
		void SharedQueueReader();
		void GetIPAddressFromLocalAdaptor(const std::string& strNetworkInterface, std::string& strIpAddress);
	};

	//////////////////////////////////////////////////////////////////////
	#define CATCH_IPC \
		catch(const ipc::interprocess_exception& ex)\
		{\
			LOGERROR << "Interprocess exception, error msg: " << ex.what();\
		}

	//////////////////////////////////////////////////////////////////////
} //namespace IPC

#endif // #ifndef IPCMODULE_H_
