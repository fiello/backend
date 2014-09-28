
#ifndef IPCMODULE_H_
#define IPCMODULE_H_

//native
#include "ThreadPoolModule.h"
//thirdparty
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
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
	private: // permit no copy, explicit ctors/dtors
		CIPCModule();
		CIPCModule(const CIPCModule& src);
		CIPCModule& operator= (const CIPCModule& src);
		~CIPCModule(){;}

	public: // methods
		static CIPCModule& Instance();
		void Destroy();
		bool IsFirstInstance();
		void CreateMessageQueue();
		void StartListener(size_t iNumberOfThreads);
		void StartSelector();
		void SetIPSettings(const std::string& strNetworkInterface, int iPort);

	private: //members
		static CIPCModule* m_pSelf;
		static ipc::interprocess_mutex m_ipcAccess;
		boost::shared_ptr<ipc::message_queue> m_spSharedMsgQueue;
		boost::shared_ptr<CThreadPoolModule> m_spThreadModule;
		std::string m_strNetworkInterface;
		int m_iPort;
		int m_iPipeFDs[2];
		std::list<int> m_listPendingSock;

	private: //methods
		void SharedQueueReader();
		void NotifySelector(int iNewDescriptor);
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
