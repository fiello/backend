#pragma once

#include <list>
#include <string>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

class Connection;
class Request;
class DataFile;

// Constant value define number of working threads
// Now it is hardcoded for compile time but can be taken to configuration parameter if required
#ifndef QUEUE_WORKERS_NUMBER
#define QUEUE_WORKERS_NUMBER 4
#endif

// Class TestQueue
// Receives text requests with associated connection.
// Stores them and execute in several working threads
// Can be set to maintenance mode to stop requests processing (but not getting) until unsetting it
class TestQueue
{
public:
   // Constructor getting data file object to execute requests with,
   // configured delay and start-up maintenance mode
   // Doesn't start processing. Call start method instead
   TestQueue(DataFile &data, int msecDelay, bool maintenance = false);

   // Sets new delay. Doesn't affect request in progress.
   void setDelay(int msecDelay);
   // Set maintenance mode
   // if value == true - finish started request and stop queued request processing until
   //    value == false - continue queued requests processing
   // Not the same as start/stop because doesn't destroy working threads but just pause them
   void setMaintenance(bool value);

   // Start requests processing in working threads
   void start();
   // Finish running requests, stop working threads and wait for their finishing
   void stop();

   // Notifies queue about closed connection
   // Used to update requests to avoid response sending
   void connectionClosed(Connection *conn);

   // Adds text request with assotiated client to the queue 
   void addRequest(const std::string &request, Connection *conn = 0);
   
protected:
   mutable boost::mutex _mutex;
   boost::condition_variable _condition;

   // Main thread function used for each working thread
   // workerId is an unique number of worker thread
   void _processRequests(int workerId);
   
   // Array of working threads
   boost::thread _workers[QUEUE_WORKERS_NUMBER];

   // Data file object to execute commands with
   DataFile &_data;
   // Configured delay (see Request class description)
   int _delay;
   // Maintenance mode
   bool _maintenance;
   // Flag used to stop running threads
   bool _stopping;
   
   // List of pending requests
   std::list<Request*> _pendingRequests;
   // List of requests being executed
   std::list<Request*> _runningRequests;
};

