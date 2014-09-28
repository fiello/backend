/**
 *  \file
 *  \brief     SocketAddressHolder class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_SOCKET_ADDRESS_HOLDER_H
#define CS_NETWORK_SOCKET_ADDRESS_HOLDER_H

#include <boost/noncopyable.hpp>
#include <netinet/in.h>
#include <string>

namespace cs
{
namespace network
{

/**
 *  \class     cs::network::SocketAddressHolder
 *  \brief     Holder class for socket address structure
 *  \details   Help carrying socket address structure and provide some simply interface
 *             to work with it
 */
class SocketAddressHolder : boost::noncopyable
{
public:
   /**
    * Constructor, cleans internal address structure
    */
   SocketAddressHolder();

   /**
    * Constructor, converts input pair of address:port to appropriate
    * structure. Caller must be prepared to handle exception in case if
    * input argument is invalid.
    * @param address - string with the IPv4 address. String is not checked for
    *                  IPv4 validity therefore it's a caller responsibility to
    *                  validate address
    * @param port - port number
    */
   SocketAddressHolder(const std::string& address, const unsigned int port);

   /**
    * Overloaded operator() to get access to internal address
    * @returns - pointer to the constant structure with address
    */
   operator const sockaddr*() const;

   /**
    * Overloaded assignment operator to store external address structure
    * in this holder
    * @param address - const reference to external address
    * @returns - reference to current instance of SocketAddressHolder
    */
   SocketAddressHolder& operator =(const sockaddr &address);

   /**
    * Helper method to get size of internal structure with the address
    * @returns - sizeof() taken from address structure
    */
   socklen_t GetSize() const;

private:
   /// address structure itself
   sockaddr m_address;
};

} // namespace network
} // namespace cs

#endif // CS_NETWORK_SOCKET_ADDRESS_HOLDER_H
