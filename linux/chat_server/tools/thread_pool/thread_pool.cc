/**
 *  \file
 *  \brief     ThreadPool class implementation
 *  \details   Holds implementation of the ThreadPool class and some auxiliary functionality
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "thread_pool.h"
#include <logger/logger.h>
#include <common/exception_dispatcher.h>
// third-party
#include <boost/bind.hpp>

namespace cs
{
namespace thread_pool
{

/// unique id of thread pool, transparent through the whole application
static int ThreadPoolId = -1;

/// unique id of worker thread, transparent through the whole application
/// and all instances of thread pools
static int PoolWorkerQueueId = -1;

/////////////////////////////////////////////////////////////////
// ThreadPool

ThreadPool::ThreadPool(const int maxThreadCount)
   : m_maxThreadCount(maxThreadCount)
   , m_poolId(++ThreadPoolId)
   , m_shutdownRequested(false)
   , m_isPoolInitialized(false)
{}

void ThreadPool::Initialize()
{
   WorkerQueuePtr worker;
   for (int i = 0; i < m_maxThreadCount; ++i)
   {
      worker.reset( new WorkerQueue(*this) );
      worker->Initialize();
      m_workerQueueStorage.push_back(worker);
   }
   m_isPoolInitialized = true;
   LOGDBG << "Thread pool #" << m_poolId << " is initialized";
}

void ThreadPool::Shutdown()
{
   if (!m_shutdownRequested && m_isPoolInitialized)
   {
      LOGDBG << "Thread pool #" << m_poolId << " started shutdown";
      m_shutdownRequested = true;

      for (WorkerQueueStorage::const_iterator it = m_workerQueueStorage.begin();
         it != m_workerQueueStorage.end();
         ++it)
      {
         // light trick to shutdown those threads which could miss single notification because
         // there didn't enter the wait condition by the moment we called notification.
         m_queueEvent.notify_all();
         (*it)->Shutdown();
      }
      m_isPoolInitialized = false;
   }
}

void ThreadPool::AddTask(ThreadTask task)
{
   if (!m_isPoolInitialized)
      THROW_BASIC_EXCEPTION(result_code::eNotReady) << "Component is not initialized!";

   if (m_shutdownRequested)
   {
      LOGWRN << "Unable to handle incoming task during system shutdown";
      return;
   }

   {
      LOCK lock(m_taskAccessGuard);
      m_taskList.push_back(task);
   }

   m_queueEvent.notify_all();
}

int ThreadPool::GetPoolId() const
{
    return m_poolId;
}

result_t ThreadPool::TryGetNewTask(WorkerQueue* caller, ThreadTask& newTask)
{
   LOCK lock(m_taskAccessGuard);

   while (m_taskList.empty())
   {
      // The first time each worker enters this condition and notifies
      // caller that it has started. Need this trick to ensure that worker
      // completely started and ready to wait for queue event (new task).
      // This notification is wired to the WorkerQueue::Initialize method
      if (!m_isPoolInitialized)
         caller->NotifyWorkerStarted();

      m_queueEvent.wait(lock);

      if (m_shutdownRequested)
         return result_code::eNotFound;
   }

   newTask = m_taskList.front();
   m_taskList.pop_front();
   return result_code::sOk;
}

/////////////////////////////////////////////////////////////////
// WorkerQueue

ThreadPool::WorkerQueue::WorkerQueue(ThreadPool& parentPool)
   : m_parentPool(parentPool)
   // magic number means 2 threads: worker thread and the caller
   // who will invoke WorkerQueue::Initialize
   , m_threadBarrierSync(2)
   , m_shutdownRequested(false)
   , m_queueId(++PoolWorkerQueueId)
{}

void ThreadPool::WorkerQueue::Initialize()
{
   m_workerThread.reset( new boost::thread( boost::bind(&WorkerQueue::ProcessTasks, this) ) );

   // block unless we have a signal from worker thread that it has entered necessary state
   // in ThreadPool::TryGetNewTasks method
   m_threadBarrierSync.wait();

   LOGDBG << "Worker #" << m_queueId
          << " of pool #" << m_parentPool.GetPoolId() << " is initialized";
}

void ThreadPool::WorkerQueue::NotifyWorkerStarted()
{
   m_threadBarrierSync.wait();
}

void ThreadPool::WorkerQueue::Shutdown()
{
   m_shutdownRequested = true;
   if (m_workerThread->timed_join(boost::posix_time::seconds(5)) == false)
   {
      // TODO: current implementation implies that Pool is stopped at the very end of
      // application. So just detach thread and exit - scheduler will kill zombie thread himself.
      // Probably need to consider another mechanism for graceful shutdown
      LOGERR << "Unable to stop worker thread #" << m_queueId
             << " of pool #" << m_parentPool.GetPoolId() << ", force detach";
      m_workerThread->detach();
   }
   LOGDBG << "Worker #" << m_queueId
          << " of pool #" << m_parentPool.GetPoolId() << " is shut down";
}

void ThreadPool::WorkerQueue::ProcessTasks()
{
   ThreadTask task;
   while (!m_shutdownRequested)
   {
      // current thread has nothing to work with, ask the parent pool
      if (m_parentPool.TryGetNewTask(this, task) != result_code::sOk)
         break;

      LOGDBG << "Exec task in queue #" << m_queueId << " of pool #" << m_parentPool.GetPoolId();
      task();

      // Task ptr itself lives in outer scope, thus need to reset the
      // last item from localQueue manually
      task.clear();
   }
}

} // namespace thread_pool
} // namespace cs
