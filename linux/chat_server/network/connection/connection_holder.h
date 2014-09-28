/**
 *  \file
 *  \brief     ConnectionHolder class declaration
 *  \details   Holds ConnectionHolder class implementation with some auxiliary type structures
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_CONNECTION_HOLDER_H
#define CS_NETWORK_CONNECTION_HOLDER_H

#include <network/socket/socket_wrapper.h>
#include <common/result_code.h>
#include <core/data_processing/task.h>
#include <logger/logger.h>
// third-party
#include <boost/thread/mutex.hpp>
#include <boost/weak_ptr.hpp>

namespace cs
{
namespace network
{

class ConnectionHolder;
struct ConnectionCarrier;
typedef boost::shared_ptr<ConnectionHolder> ConnectionHolderPtr;
typedef boost::weak_ptr<ConnectionHolder> ConnectionWeakPtr;
typedef boost::shared_ptr<ConnectionCarrier> ConnectionCarrierPtr;

/**
 *  \struct    cs::network::ConnectionCarrier
 *  \brief     Helper structure to hold weak reference to the connection
 *  \details   Helper structure to be used with epoll object. It holds weak reference
 *             to connection object and therefore can be passed to epoll object and retrieved
 *             back from it.
 */
struct ConnectionCarrier
{
   ConnectionWeakPtr holder;
};


/**
 *  \class     cs::network::ConnectionHolder
 *  \brief     Helper class that presents 'connection' entity
 *  \details   Presents 'connection' entity as a summary of several items:
 *              - instance of SocketWrapper for the opened socket
 *              - username associated with this connection/socket
 *              - buffer with raw data received from the network
 *              - several helper flags and methods to simplify work with the object
 */
class ConnectionHolder : public boost::noncopyable
{
public:
   /**
    * Constructor
    * @param socket - smart object with the SocketWrapper to be associated with this
    *                 connection
    * @param isListeningSocket - flag if this should be a listening socket
    */
   ConnectionHolder(SocketWrapperPtr socket, const bool isListeningSocket = false);

   /**
    * Destructor that posts a farewell message to all chat participants about client
    * disconnect
    */
   ~ConnectionHolder();

   /**
    * Helper function to see if connection is holding a listening socket and therefore
    * can be used to establish new connections
    * @returns - true if it is a connection with listening socket, false otherwise
    */
   bool IsListeningSocket() const;

   /**
    * Helper function to see if connection has already been closed
    * @returns - true if connection is closed, false otherwise
    */
   bool IsConnectionClosed() const;

   /**
    * Read data from wrapped socket and append it to the exisitng buffer
    * @returns - result code of the operation
    *             - sOk if data was read correctly and appended to the existing buffer
    *             - eBufferOverflow if new data being appended to existing buffer will
    *               exceed allowed message size
    *             - eConnectionClosed if socket was closed during the read procedure
    */
   result_t ReadAndAppendSocketData();
   result_t GetNextSocketData(std::string& data);
   void SetUsername(const std::string& newUsername = "");
   std::string GetUsername() const;
   void Close();
   void SetConnectionCarrier(ConnectionCarrierPtr carrier);

   /// Functions to work with Socket Wrapper

   /**
    * Helper function to understand if wrapped socket is valid
    * @returns - true if wrapped socket is valid, false otherwise
    */
   bool IsSocketValid() const;

   /**
    * Get descriptor of the wrapped socket
    * @returns - descriptor of the wrapped socket
    */
   SocketDescriptor GetSocketDescriptor() const;

   /**
    * Accept new incoming connection on the given socket
    * @param socketAddress - out structure with remote client address
    * @returns - descriptor of the new socket opened to interact with remote client
    */
   SocketDescriptor AcceptNewConnection(SocketAddressHolder& socketAddress);

   /**
    * Write data to the wrapped socket
    * @param dataBuffer - reference to the string with data available for writing
    * @returns - number of bytes written to the socket. Value -1 is returned if
    *            internal system error occurred during the write procedure.
    */
   ssize_t WriteDataToSocket(const std::string& dataBuffer);

private:
   typedef boost::lock_guard<boost::mutex> LOCK;

   /// smart object with strong reference to the carreir. Each connection
   /// controls timespan of the carrier that holds this connection
   ConnectionCarrierPtr    m_carrier;
   /// socket wrapper that this connection is associated with
   SocketWrapperPtr        m_socketWrapper;
   /// sync object to guard access to the socket data
   boost::mutex            m_socketDataAccessGuard;
   /// flag that indicates if current connection is holding a listening socket
   bool                    m_isListeningSocket;
   /// flag that indicates if connection is closed
   bool                    m_isConnectionClosed;
   /// string that holds raw data received from socket
   std::string             m_socketData;
   /// string that holds username associated with this connection/socket
   std::string             m_username;
};

} // namespace network
} // namespace cs

#endif // CS_NETWORK_CONNECTION_HOLDER_H
