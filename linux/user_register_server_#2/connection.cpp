
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <boost/thread/locks.hpp>

#include "logger.h"
#include "connection.h"

typedef boost::lock_guard<boost::recursive_mutex> RLOCK;

// Constructor
// Stores passed descriptor internally
Connection::Connection(int fd)
   : _fd(fd)
{
}

Connection::~Connection()
{
   // Don't close _fd here. For UDP - it's listening (server's) socket
}

// Descriptor getter
int Connection::fd() const
{
   RLOCK lock(_mutex);
   return _fd;
}

// Constuctor
// fd is a client's socket for sending responses to
TcpConnection::TcpConnection(int fd)
   : Connection(fd)
{
}

// Add data recevied from client to internal buffer
void TcpConnection::addText(const char* text)
{
   RLOCK lock(_mutex);
   _buffer += text;
}

// Get request from data for each CRLF is received. Store the rest for more data.
bool TcpConnection::getNextRequest(std::string &request)
{
   RLOCK lock(_mutex);

   size_t pos = _buffer.find("\r\n");
   if(pos == std::string::npos)
      return false;

   request = _buffer.substr(0, pos);
   _buffer.erase(0, pos+2);

   return true;
}

// Send response to associated socket
bool TcpConnection::send(const char *response)
{
   bool result = false;
   if(response)
   {
      RLOCK lock(_mutex);
      // LOG_DEBUG("Sending data: '%s'", response);
      result = -1 < ::send(_fd, response, strlen(response), 0);
   }
   return result;
}


// Constructor
// fd - is a UDP SERVER socket to send response from
// clientAddr and clientPort are used for sending response to
UdpConnection::UdpConnection(int fd, int clientAddr, int clientPort)
   : Connection(fd),
     _addr(clientAddr),
     _port(clientPort)
{
}

// Send response to stored client address:port from stored fd
// !!! after call to send the UdpConnection object is invalid (object is self deleted)
bool UdpConnection::send(const char *response)
{
   bool result = false;
   if(response)
   {
      RLOCK lock(_mutex);
      // LOG_DEBUG("Sending data: '%s'", response);
      struct sockaddr_in dst;
      dst.sin_family = AF_INET;
      dst.sin_addr.s_addr = _addr;
      dst.sin_port = _port;
      result = -1 < ::sendto(_fd, response, strlen(response), 0, (const sockaddr*)&dst, sizeof(struct sockaddr_in));
   }
   delete this;
   return result;
}

