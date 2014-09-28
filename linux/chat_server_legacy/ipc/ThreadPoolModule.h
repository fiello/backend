
#ifndef THREADPOOLMODULE_H_
#define THREADPOOLMODULE_H_

#include "Logger.h"
//thirdparty
#include <list>
#include <map>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <threadpool/threadpool.hpp>
#include <boost/thread/mutex.hpp>

namespace IPC
{
	//////////////////////////////////////////////////////////////////////////
	class CBaseSocket;
	class CReceiveTask;
	class CThreadPoolModule;
	typedef boost::shared_ptr<CReceiveTask> spCReceiveTask;
	typedef boost::shared_ptr<CBaseSocket> spCBaseSocket;
	typedef std::map<int,spCReceiveTask> tMapReceiveTasks;
	typedef std::list<int> tListActiveSockets;
	typedef tListActiveSockets::iterator tIterListActiveSockets;
	typedef tMapReceiveTasks::iterator tIterMapReceiveTasks;
	typedef boost::function<void(int)> tFuncSelectorNotifier;

	//////////////////////////////////////////////////////////////////////////
	class CBaseSocket
	{
	private:
		CBaseSocket(){;}

	public:
		CBaseSocket(int iSocketDesc);
		virtual ~CBaseSocket();
		int GetDescriptor() {return m_iSocketDescriptor;}
		operator int() const {return m_iSocketDescriptor;}

	private:
		int m_iSocketDescriptor;
	};

	//////////////////////////////////////////////////////////////////////////
	class CReceiveTask
	{
	private:
		CReceiveTask(){;}

	public:
		CReceiveTask(int iSocketDesc):
			m_bIsTaskCompleted(false),
			m_bIsNotified(false)
		{
			m_spSocket.reset(new CBaseSocket(iSocketDesc));
		}

		void ReceiveData();
		void AssignSelectorNotifier(const tFuncSelectorNotifier& func) {m_funcHandler = func;}
		void AssignParrent(CThreadPoolModule* ptrParent) { m_ptrThreadPool = ptrParent; }
		bool IsCompleted() { return m_bIsTaskCompleted; }
		int GetDescriptor() { return m_spSocket->GetDescriptor(); }

	private:
		CThreadPoolModule* m_ptrThreadPool;
		spCBaseSocket m_spSocket;

		bool m_bIsTaskCompleted;
		bool m_bIsNotified;
		tFuncSelectorNotifier m_funcHandler;
	};

	//////////////////////////////////////////////////////////////////////////
	class CThreadPoolModule
	{
	private: //permit no copy
		CThreadPoolModule(){;}
		CThreadPoolModule(const CThreadPoolModule& src);
		CThreadPoolModule& operator= (const CThreadPoolModule& src);

	public: //methods
		CThreadPoolModule(size_t iPoolSize);
		void AddTask(const spCReceiveTask& spTask);
		void RenewTask(const spCReceiveTask& spTask);
		bool FindTaskBySocket(int iSocketFD, spCReceiveTask& spTarget);
		void RemoveTaskBySocket(int iSocketFD);
		void GetConnectionList(tListActiveSockets& copy);

	private: //members
		tMapReceiveTasks m_mapReceiveTasks;
		tListActiveSockets m_listActiveSockets;
		boost::shared_ptr<boost::threadpool::pool> m_spThreadPool;
		boost::mutex m_Access;
	};

	//////////////////////////////////////////////////////////////////////////
} //namespace IPC
#endif /* THREADPOOLMODULE_H_ */
