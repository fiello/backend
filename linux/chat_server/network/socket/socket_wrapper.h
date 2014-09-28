/**
 *  \file
 *  \brief     SocketWrapper class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_SOCKET_WRAPPER_H
#define CS_NETWORK_SOCKET_WRAPPER_H

#include "socket_address_holder.h"
#include <network/descriptor.h>
// third-party
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <netinet/tcp.h>
#include <netinet/in.h>

namespace cs
{
namespace network
{

class SocketWrapper;
typedef boost::shared_ptr<SocketWrapper> SocketWrapperPtr;

/**
 *  \class     cs::network::SocketWrapper
 *  \brief     Wrapper class for the system socket
 *  \details   Implement RAII pattern for the system socket and provide
 *             simplified interface for interaction with the wrapped socket.
 */
class SocketWrapper : boost::noncopyable
{
public:
   /**
    * Constructor, wrap existing socket. Caller must be prepared to handle
    * exception in case of invalid socket descriptor.
    * @param socket - descriptor of the system socket to be wrapped
    */
   SocketWrapper(const SocketDescriptor socket);

   /**
    * Constructor, create a new socket and wrap its descriptor. For more information
    * about parameters please refer to 'man 7 socket'. Caller must be prepared to handle
    * exception in case if class was unable to open a new socket.
    * @param domain - socket family (e.g. AF_INET)
    * @param type - type of the socket (e.g. SOCK_STREAM for TCP socket)
    * @param protocol - protocol type (e.g. IPPROTO_IP)
    */
   SocketWrapper(const int domain, const int type, const int protocol);

   /**
    * Destructor, close socket connection.
    */
   ~SocketWrapper();

   /**
    * Set specific option for the wrapped socket. See 'man 2 setsockopt' for more info.
    * Caller must be prepared to handle exception in case if option was not set due
    * to some system error
    * @param level - protocol level that the option will be set at
    * @param optionName - id of the option to be set
    * @param optionValue - value of the option to be set
    */
   void SetSocketOption(const int level, const int optionName, const int optionValue);

   /**
    * Set socket to non-blocking state
    */
   void SetNonblocking();

   /**
    * Accept new incoming connection on the socket (if it's a listening one) and provides
    * caller with remote client address description. Caller must be prepared to handle exception
    * in case if new connection was not accepted due to some system error
    * @param socketAddress - out structure with remote client address
    * @returns - descriptor of the new socket opened to interact with remote client
    */
   SocketDescriptor Accept(SocketAddressHolder& socketAddress);

   /**
    * Bind wrapped socket to the given address. Caller must be prepared to handle exception
    * in case if bind procedure failed
    * @param address - address that the wrapped socket should be binded to
    */
   void Bind(const SocketAddressHolder& address);

   /**
    * Set wrapped socket to the listening state. Bind procedure must be performed prior to this
    * call. Caller must be prepared to handle exception if listen procedure failed to complete
    * correctly
    * @param backlogSize - size of the local queue for incoming connections
    */
   void Listen(const int backlogSize);

   /**
    * Read data from socket. Caller must be prepared to handle an exception if read
    * procedure failed.
    * @param dataBuffer - pointer to the buffer where data will be written to
    * @param bytesCount - maximum size of the dataBuffer argument in bytes
    * @returns - number of bytes read from socket. Value -1 is returned if given
    *            buffer size was not enough to read all available data.
    */
   ssize_t Read(void *dataBuffer, const size_t bytesCount);

   /**
    * Write data to the socket.
    * @param dataBuffer - reference to the string with data available for writing
    * @returns - number of bytes written to the socket. Value -1 is returned if
    *            internal system error occurred during the write procedure.
    */
   ssize_t Write(const std::string& dataBuffer);

   /**
    * Close current socket
    */
   void Close();

   /**
    * Get descriptor of the wrapped socket
    */
   SocketDescriptor GetDescriptor() const;

   /**
    * Verifies if current socket has valid descriptor and it is
    * not closed
    */
   bool IsValid() const;

private:
   /// descriptor of the system socket to be wrapped
   SocketDescriptor  m_socket;
   /// flag that socket is closed
   bool              m_isClosed;
};

} // namespace network
} // namespace cs

#endif // CS_NETWORK_SOCKET_WRAPPER_H
