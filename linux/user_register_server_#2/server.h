#pragma once

#include <netinet/in.h>
#include <string>
#include <boost/thread.hpp>
#include "queue.h"

class TestQueue;

// TestServer class
//
// Creates 2 server side connections: TCP and UDP running in 2 parallel threads
// TCP server can accept multiple client connections
// UDP server can accept requests from different connections
// TestQueue is used for requests processing and responses generation
//
// Usage: create TestServer with TCP and UDP interfaces and ports specified
//        execute start method passing pointer to configured TestQueue object
//        server executes queue processing itself so it is not necessary to start it before
class TestServer
{
public:
   // Constructor
   // Only stores server configuration
   // Call start() method to start requests processing
   TestServer(const std::string &tcpIf, int tcpPort,
              const std::string &udpIf, int udpPort);

   // Creates 2 threads for TCP and UDP servers and starts queue
   // Each request is passed to queue for processing
   void start(TestQueue *queue);

   // Stops requests processing
   void stop();

protected:
   struct in_addr _tcpAddr;
   struct in_addr _udpAddr;
   int _tcpPort;
   int _udpPort;

   // Queue that stores and processes requests in its own working threads
   TestQueue *_queue;

   // Internal server thread functions
   void _tcpServer();
   void _udpServer();

   // Thread handlers
   boost::thread _tcpThread;
   boost::thread _udpThread;
};

