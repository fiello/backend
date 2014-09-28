/**
 *  \file
 *  \brief     ThreadPool class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_THREAD_POOL_H
#define CS_THREAD_POOL_H

#include <common/result_code.h>
// third-party
#include <boost/function.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>

namespace cs
{
namespace thread_pool
{

/// type of the atomic task that ThreadPool is capable handling of
typedef boost::function<void()> ThreadTask;

/**
 *  \class     cs::thread_pool::ThreadPool
 *  \brief     Simple implementation of thread pool
 *  \details   Class that starts given number of threads at once and keeps
 *             them on hold unless new tasks are arrived. Type of acceptable tasks is
 *             described above. Each thread has no memory and stores/executes only one task at
 *             a time. Task functor is purged after the execution.
 */
class ThreadPool
{
public:
   /**
    * Constructor
    * @param maxThreadCount - maximum number of threads in this pool
    */
   ThreadPool(const int maxThreadCount);

   /**
    * Execute thread pool initialization routine:
    *  - create workers by the number of maxThreadCount value
    *  - initialize each worker and hold it in internal container for further management
    */
   void Initialize();

   /**
    * Execute thread pool shutdown routine. Shut down each worker one by one and emit flag
    * that thread is not initialized anymore
    */
   void Shutdown();

   /**
    * Main method to add new tasks to the thread.
    * @param task - functor with zero input parameters that will be stored in a pool queue
    *               and then processed by the nearest free worker thread. Each task is
    *               executed only once and will be purged right after execution is over.
    */
   void AddTask(ThreadTask task);

   /**
    * Method to get thread id - will be used by the worker thread to print proper
    * trace string to log
    * @returns thread pool id
    */
   int GetPoolId() const;

private:
   class WorkerQueue;	// need forward declaration for typedef below
   typedef boost::unique_lock<boost::mutex> LOCK;
   typedef boost::shared_ptr<WorkerQueue> WorkerQueuePtr;
   typedef std::list<WorkerQueuePtr> WorkerQueueStorage;

   /// private method available for worker thread to captrue new task to process
   result_t TryGetNewTask(WorkerQueue* caller, ThreadTask& newTask);

   /// list of unprocessed tasks in this pool
   std::list<ThreadTask>      m_taskList;
   /// container which holds all workers
   WorkerQueueStorage         m_workerQueueStorage;
   /// mutex needed to guard access to list with unprocessed tasks. Includes sync of
   /// external callers of the 'AddTask' method and internal worker threads
   boost::mutex               m_taskAccessGuard;
   /// event to notify workers about new tasks or thread pool shutdown
   boost::condition_variable  m_queueEvent;

   /// maximum number of threads per thread pool
   const int                  m_maxThreadCount;
   /// helper id of the pool which could help in debugging if applications uses several pools
   int                        m_poolId;
   /// flag that shutdown was requested
   bool                       m_shutdownRequested;
   /// flag that pool is initialized
   bool                       m_isPoolInitialized;

private:

   /// internal helper class which acts as a wrapper for worker thread
   /// and provides simple management interface to the parent object of ThreadPool
   class WorkerQueue
   {
   public:
      /// Constructor which requires a reference to a parent ThreadPool object
      WorkerQueue(ThreadPool& parentPool);
      /// Initialize current worker queue
      void Initialize();
      /// Request shutdown for current worker queue
      void Shutdown();
      /// public method that will be invoked by
      void NotifyWorkerStarted();
      /// Worker thread routine which grabs new tasks and exectues them
      void ProcessTasks();

   private:
      /// reference to the parent thread pool object. Need it to get access to newly
      /// dispatched tasks
      ThreadPool&                         m_parentPool;
      /// barrier to sync worker threads during startup and shutdown in order to avoid lockups
      boost::barrier                      m_threadBarrierSync;
      /// flag that shutdown was requested
      bool                                m_shutdownRequested;
      /// helper variable to track queue number during startup and shutdown
      int                                 m_queueId;
      /// wrapper upon the worker thread
      boost::scoped_ptr<boost::thread>    m_workerThread;
   };
};


} // namespace thread_pool
} // namespace cs

/**
 *  \namespace    cs::thread_pool
 *  \brief        Holds ThreadPool class declaration and implementation
 */

#endif // CS_THREAD_POOL_H
