
#include <boost/thread/locks.hpp>

#include "server.h"
#include "request.h"
#include "logger.h"
#include "queue.h"

typedef boost::unique_lock<boost::mutex> LOCK;

// Constructor getting data file object to execute requests with,
// configured delay and start-up maintenance mode
// Doesn't start processing. Call start method instead
TestQueue::TestQueue(DataFile &data, int msecDelay, bool maintenance)
   : _data(data),
     _delay(msecDelay),
     _maintenance(maintenance),
     _stopping(false)
{
}

// Sets new delay. Doesn't affect request in progress.
void TestQueue::setDelay(int msecDelay)
{
   LOCK lock(_mutex);
   _delay = msecDelay;
}

// Set maintenance mode
// if value == true - finish started request and stop queued request processing until
//    value == false - continue queued requests processing
// Not the same as start/stop because doesn't destroy working threads but just pause them
void TestQueue::setMaintenance(bool value)
{
   LOCK lock(_mutex);
   _maintenance = value;
   if(!_maintenance)
      _condition.notify_all();
}

// Start requests processing in working threads
void TestQueue::start()
{
   _stopping = false;
   for(int i = 0; i < QUEUE_WORKERS_NUMBER; ++i)
   {
      if(_workers[i].get_id() == boost::thread::id())
         _workers[i] = boost::thread(&TestQueue::_processRequests, this, i);
   }
}

// Finish running requests, stop working threads and wait for their finishing
void TestQueue::stop()
{
   {
      LOCK lock(_mutex);
      _stopping = true;
      _condition.notify_all();
   }
   
   for(int i = 0; i < QUEUE_WORKERS_NUMBER; ++i)
   {
      if(_workers[i].get_id() != boost::thread::id())
         _workers[i].join();
   }
}

// Auxillary functor for connection invalidation in a list of requests
struct InvalidatorFO
{
   // Store invalidated conection 
   InvalidatorFO(Connection *connection) : _connection(connection) {};
   // Invalidate connection if match
   void operator()(Request *request)
   {
      if(request->connection() == _connection)
         request->invalidateConnection();
   }
private:
   Connection *_connection;
};

// Notifies queue about closed connection
// Used to update requests to avoid response sending
void TestQueue::connectionClosed(Connection *conn)
{
   LOCK lock(_mutex);
   InvalidatorFO invalidator(conn);
   std::for_each(_pendingRequests.begin(), _pendingRequests.end(), invalidator);
   std::for_each(_runningRequests.begin(), _runningRequests.end(), invalidator);
}

// Adds text request with assotiated client to the queue 
void TestQueue::addRequest(const std::string &request, Connection *conn)
{
   LOCK lock(_mutex);
   LOG_DEBUG("Adding request to queue: '%s'", request.c_str());
   _pendingRequests.push_back(new Request(request, conn));
   _condition.notify_one();
}

// Main thread function used for each working thread
// workerId is an unique number of worker thread
void TestQueue::_processRequests(int workerId)
{
   TestLogger *log = TestLogger::instance();
   log->debug("Queue worker #%d started.", workerId);
   while(1)
   {
      Request *request = 0;
      {
         LOCK lock(_mutex);
         if(_stopping)
            break;
         if(_maintenance || _pendingRequests.empty())
         {
            // Wait if queue is empty or maintenance mode is on
            _condition.wait(lock);
         }
         else
         {
            // Extract request from the queue
            request = _pendingRequests.front();
            _pendingRequests.pop_front();
            // Put it into running list
            _runningRequests.push_back(request);
         }
      }
      if(request)
      {
         // Execute request
         log->debug("Worker #%d: processing next reqeust", workerId);
         request->execute(_data, _delay);
         LOCK lock(_mutex);
         // Remove it from the list of running requests
         _runningRequests.remove(request);
         // And delete
         delete request;
      }
   }
   LOG_DEBUG("Worker #%d stopped", workerId);
}
