#pragma once

#include <string>
#include <boost/thread/recursive_mutex.hpp>

// Class Connection
// Base class declaring send functionality and descriptor handling
class Connection
{
public:
   // Constructor
   // Stores passed descriptor internally
   Connection(int fd);
   // Virtual destructor declaration for correct derived objects deletion
   virtual ~Connection();

   // Descriptor getter
   int  fd() const;

   // Pure virtual function that is Connection specific
   virtual bool send(const char *response) = 0;
   
   // Additional send ficntion that uses first one for std::string sending
   virtual bool send(const std::string &response)
   {
      return send(response.c_str());
   };

protected:
   // Synchronization locks
   mutable boost::recursive_mutex _mutex;
   mutable boost::recursive_mutex _busy_mutex;

   // Stored descriptor
   int _fd;
};

// Class TcpConnection
// Implements TCP connections with sending message to stored socket
// Stores incomplete data received from associated client and forms requests on CRLF receiving
class TcpConnection: public Connection
{
public:
   // Constuctor
   // fd is a client's socket for sending responses to
   TcpConnection(int fd);
   virtual ~TcpConnection() {};

   // Add data recevied from client to internal buffer
   void addText(const char *);
   // Get request from data for each CRLF is received. Store the rest for more data.
   bool getNextRequest(std::string &request);

   // Send response to associated socket
   bool send(const char *response);
   
protected:
   // Internal buffer for incomplete requets
   std::string _buffer;
};

// Class UdpConnection
// Implements UDP connection with sending responses to specified client's address:port from stored descriptor
// !!! UdpConnection is disposable. I.e. can be used only for single response sending and self deletes in send()
class UdpConnection: public Connection
{
public:
   // Constructor
   // fd - is a UDP SERVER socket to send response from
   // clientAddr and clientPort are used for sending response to
   UdpConnection(int fd, int clientAddr, int clientPort);
   virtual ~UdpConnection() {}

   // Send response to stored client address:port from stored fd
   // !!! after call to send the UdpConnection object is invalid (object is self deleted)
   bool send(const char *response);
   
protected:
   // Client's destination
   int _addr;
   int _port;
};
