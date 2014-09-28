/**
 *  \file
 *  \brief     SocketAddressHolder class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "socket_address_holder.h"
#include <common/exception_dispatcher.h>
// third-party
#include <string.h>
#include <arpa/inet.h>

namespace cs
{
namespace network
{

SocketAddressHolder::SocketAddressHolder()
{
   ::memset(&m_address, 0, sizeof(m_address));
}

SocketAddressHolder::SocketAddressHolder(const std::string &localAddress, const unsigned int localPort)
{
   CHECK_ARGUMENT(!localAddress.empty(), "Local address cannot be empty!");

   ::memset(&m_address, 0, sizeof(m_address));
   sockaddr_in* address = (sockaddr_in*)&m_address;
   address->sin_family = AF_INET;
   address->sin_port = ::htons(localPort);
   address->sin_addr.s_addr = ::inet_addr(localAddress.c_str());
}

SocketAddressHolder::operator const sockaddr*() const
{
   return &m_address;
}

SocketAddressHolder& SocketAddressHolder::operator =(const sockaddr &address)
{
   m_address = address;
   return *this;
}

socklen_t SocketAddressHolder::GetSize() const
{
   socklen_t length = sizeof(m_address);
   return length;
}

} // namespace network
} // namespace cs
