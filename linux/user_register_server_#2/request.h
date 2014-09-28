#pragma once

#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

class Connection;
class DataFile;

// Delay response sending policies

// Send response DELAY msec after request received.
// If number of clients more than number of working threads than some requests will execute without dalay
// due to request execution start will be delayed waiting in the queue
#define DELAY_NO_EARLIER 0 

// Send response DELAY msec after request execution started
// (includes time for request parsing and datafile communication)
#define DELAY_EXECUTION  1 

// Just add sleep for DELAY msec after request parsing and execution before response sending
// Causes biggest delay
#define DELAY_ADD_SLEEP  2 

// Selected delay policy
#ifndef DELAY_POLICY
#define DELAY_POLICY DELAY_EXECUTION
#endif

// Class Request
// Parses text request, executes it using passed DataFile object and sends response to client's connection
// On creation only response and connection are stored
// Use execute method for real request processing
class Request
{
public:
   // Constructor
   // Stores pased request and client connection
   Request(const std::string &request, Connection *conn = NULL);

   // Invalidate client's connection: request should be processed but response is to be discarded
   // Used for client's connections closed before or during request execution
   void invalidateConnection();

   // Client's connection getter
   const Connection *connection() const;

   // Request getter
   const std::string& request() const;

   // Main processing function
   // Parse request, pass to data and send response to client
   // optional delayMsec parameter is used for response sending delay
   void execute(DataFile &data, int delayMsec = 0);

protected:
   mutable boost::mutex _mutex;
   // Test request
   std::string _request;
   // Client connection to send response to
   Connection *_connection;

#if DELAY_POLICY == DELAY_NO_EARLIER
   // Time of request creation.
   // Requered only for delay no_aerlier policy delay response realive to request receiving time
   boost::posix_time::ptime _creationTime;
#endif
};

