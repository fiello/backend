
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/ioctl.h> 
#include <sys/time.h>
#include <fcntl.h>
#include <net/if.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <set>

#include "logger.h"
#include "connection.h"
#include "server.h"

using namespace std;

// Returns ip address in case of succes and 0 (in s_addr) otherwise (won't connect to 0.0.0.0)
// Can handle interface name (e.g. eth0), hostname (e.g. localhost) or IP address (e.g. 192.168.0.1)
static struct in_addr getIpByConfigValue(const char *value);

// Constructor
// Only stores server configuration
// Call start() method to start requests processing
TestServer::TestServer(const std::string &tcpIf, int tcpPort, const std::string &udpIf, int udpPort)
   : _tcpPort(tcpPort),
     _udpPort(udpPort),
     _queue(0)
{
   TestLogger *log = TestLogger::instance();
   // Detect ip addresses of both interfaces
   _tcpAddr = getIpByConfigValue(tcpIf.c_str());
   if(_tcpAddr.s_addr)
      log->debug("Configured TCP connection: %s:%d", inet_ntoa(_tcpAddr), _tcpPort);
   else
      log->fatal("Can't determine TCP connection IP address");

   _udpAddr = getIpByConfigValue(udpIf.c_str());
   if(_udpAddr.s_addr)
      log->debug("Configured UDP connection: %s:%d", inet_ntoa(_udpAddr), _udpPort);
   else
      log->fatal("Can't determine UDP connection IP address");
}

// Creates 2 threads for TCP and UDP servers and starts queue
// Each request is passed to queue for processing
void TestServer::start(TestQueue *queue)
{
   if(!queue)
   {
      LOG_FATAL("Processing queue is not available");
      return;
   }
   _queue = queue;

   _queue->start();

   if(_tcpThread.get_id() == boost::thread::id())
      _tcpThread = boost::thread(&TestServer::_tcpServer, this);
   else
   {
      LOG_WARNING("TCP server thread already started.");
   }

   if(_udpThread.get_id() == boost::thread::id())
      _udpThread = boost::thread(&TestServer::_udpServer, this);
   else
   {
      LOG_WARNING("UDP server thread already started.");
   }
}

// Stops requetss processing
void TestServer::stop()
{
   LOG_DEBUG("Stopping processing queue");
   if(_queue)
      _queue->stop();
}

// Internal function
// Creates listening AF_INET socket of specified type (e.g. SOCK_STREAM or SOCK_DGRAM)
// and binds it to the address and port specified.
// Returns socket FD or -1 in case of error
static int createListeningServer(int type, struct in_addr inaddr, int port)
{
   int listener = -1;
   struct sockaddr_in addr;
   TestLogger *log = TestLogger::instance();

   // Create socket
   listener = socket(AF_INET, type, 0);
   if(listener < 0)
   {
      log->fatal("Failed to create socket: %s", strerror(errno));
      return -1;
   }
   
   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr = inaddr;

   int optval = 1;
   size_t optlen = sizeof(int);
   // Reuse socket to avoid "Address already in use if in waiting state"
   setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);

   // Bind socket
   if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      log->fatal("Failed to bind socket to '%s:%d': %s", inet_ntoa(inaddr), port, strerror(errno));
      close(listener);
      return -1;
   }
   return listener;
}

// Connections comparator adaptor for max descriptior finding
bool compareConnections(const Connection *c1, const Connection *c2)
{
   return c1 && c2 && ( c1->fd() < c2->fd() );
}

// TCP server thread function
// Creates server socket, accepts clients connections and passes requets to queue for processing
void TestServer::_tcpServer()
{
   TestLogger *log = TestLogger::instance();
   log->debug("Starting TCP server on %s:%d", inet_ntoa(_tcpAddr), _tcpPort);

   int listener = createListeningServer(SOCK_STREAM, _tcpAddr, _tcpPort);
   if(listener == -1)
      return;

   fcntl(listener, F_SETFL, O_NONBLOCK);
   listen(listener, SOMAXCONN);

   typedef set<TcpConnection*> Connections;
   Connections clients;

   // Main input buffer for
   char buf[1024];
   // Set of FDs listening for input
   fd_set readset;
   while(1)
   {
      // Empty set
      FD_ZERO(&readset);
      // Add server's descriptor
      FD_SET(listener, &readset);

      // Add clients' descriptors
      for(Connections::const_iterator it = clients.begin(); it != clients.end(); ++it++)
      {
         TcpConnection *conn = *it;
         if(conn)
            FD_SET(conn->fd(), &readset);
      }

      // Find biggest descriptor
      TcpConnection *maxConn = *max_element(clients.begin(), clients.end(), compareConnections);
      int mx = max(listener, maxConn ? maxConn->fd() : 0);
      // Wait for input on any socket
      if(select(mx+1, &readset, NULL, NULL, NULL) <= 0)
      {
         log->error("'select' failed: %s", strerror(errno));
         continue;
      }

      // Check for server's activity
      if(FD_ISSET(listener, &readset))
      {
         // New connection requested
         struct sockaddr_in client_info;
         socklen_t info_len = sizeof(struct sockaddr_in);
         // Get client info
         int sock = accept(listener, (struct sockaddr*)&client_info, &info_len);
         if(sock < 0)
         {
            log->fatal("'accept' failed: %s", strerror(errno));
         }
         else
         {
            log->debug("Accepted connection (%d) from: %s:%d", sock, inet_ntoa(client_info.sin_addr), client_info.sin_port);
            // Store client connection
            fcntl(sock, F_SETFL, O_NONBLOCK);
            clients.insert(new TcpConnection(sock));
         }
      }

      // Check activity on all client connections
      Connections::iterator it = clients.begin();   
      while(it != clients.end())
      {
         TcpConnection *conn = *it;
         ++it; // Do it here  because later it can be invalidated...
         int fd = conn->fd();
         if(FD_ISSET(fd, &readset))
         {
            // try to read something from socket
            int bytesRead = recv(fd, buf, 1024, 0);
               
            if(bytesRead <= 0)
            {
               // Connection closed
               log->debug("Closed connection(%d)", fd);
               // notify queue about invalidated connections
               if(_queue)
                  _queue->connectionClosed(conn);
               // Remove connection from tha list
               clients.erase(conn);
               ///delete assotiated Connection 
               delete conn;
               // And finally close file
               close(fd);
               conn = 0;
               continue;
            }
            else
            {
               // Received data from the socket
               buf[bytesRead] = '\0';
               // Assume that data can come sliced or multiple request at one message
               conn->addText(buf);
               // Get all full requests from the connection
               std::string request;
               while(conn->getNextRequest(request))
               {
                  if(_queue)
                     // Add request to the queue for processing
                     _queue->addRequest(request, conn);
               }
            }
         }
      }
   }
}

// UDP server thread function
// Creates server socket, receives requests and passes requets to queue for processing
void TestServer::_udpServer()
{
   TestLogger *log = TestLogger::instance();
   log->debug("Starting UDP server on %s:%d", inet_ntoa(_udpAddr), _udpPort);

   int listener = createListeningServer(SOCK_DGRAM, _udpAddr, _udpPort);

   if(listener == -1)
      return;

   struct sockaddr_in client_info;
   socklen_t info_len = sizeof(struct sockaddr_in);
   char buf[1024];
   int bytes_read;
   while(1)
   {
      // Read bytes from any client (in blocking mode)
      bytes_read = recvfrom(listener, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&client_info, &info_len);
      if(bytes_read > 0)
      {
         // Check for trailing "\r\n"
         if(bytes_read > 1 && buf[bytes_read - 2] == '\r' && buf[bytes_read - 1] == '\n')
            buf[bytes_read - 2] = '\0';
         else
         {
            // Fake request to let whole cycle execution and response generation
            strncpy(buf, "No trailing CRLF", sizeof(buf));
         }
         // Pass request to queue for processing
         _queue->addRequest(std::string(buf), new UdpConnection(listener, client_info.sin_addr.s_addr, client_info.sin_port));
      }
   }
}

// Returns ip address in case of succes and 0 (in s_addr) otherwise (won't connect to 0.0.0.0)
// Handles only network interface name (e.g. eth0)
static struct in_addr getIpByIfname(const char *ifname)
{
   in_addr in;
   in.s_addr = 0;
   int fd = socket(AF_INET,SOCK_DGRAM, IPPROTO_IP);
   if (fd >= 0)
   {
      struct ifreq ifr;
      memset((void *)&ifr, 0, sizeof(struct ifreq));
      strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
      if (ioctl(fd, SIOCGIFADDR, &ifr) == 0)
      {
         in = ((struct sockaddr_in &)ifr.ifr_addr).sin_addr;
      }
      else
      {
         LOG_FATAL("Can't detect IP address assigned to interface '%s'", ifname);
      }
      close(fd);
   }
   return in;
}

// Returns ip address in case of succes and 0 (in s_addr) otherwise (won't connect to 0.0.0.0)
// Can handle interface name (e.g. eth0), hostname (e.g. localhost) or IP address (e.g. 192.168.0.1)
static struct in_addr getIpByConfigValue(const char *value)
{
   struct in_addr in;
   in.s_addr = 0;

   // Step 1: assume it is IP address (e.g. 192.168.0.1)
   TestLogger *log = TestLogger::instance();
   log->debug("Trying to get IP address for '%s'", value);
   if(!inet_aton(value, &in))
   {
      // Step 2: assume it is hostname (e.g. "localhost")
      struct hostent *hent=gethostbyname(value);
      if(hent && hent->h_length > 3)
      {
         in.s_addr = *(u_int32_t*)hent->h_addr;
      }
      else
      {
         // Step 3: assume it's an interface name (e.g. "eth0" )
         in = getIpByIfname(value);
      }
   }

   if(in.s_addr != 0)
      log->debug("Detected address is: %s", inet_ntoa(in));
   else
      log->debug("Can't detect IP address of '%s'", value);
   return in;
}
