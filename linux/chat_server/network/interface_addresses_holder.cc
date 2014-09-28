/**
 *  \file
 *  \brief     InterfaceAddressesHolder class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "interface_addresses_holder.h"
#include <common/exception_dispatcher.h>

namespace cs
{
namespace network
{

InterfaceAddressesHolder::InterfaceAddressesHolder()
{
   if (::getifaddrs(&m_interfaceAddresses) == -1)
      THROW_NETWORK_EXCEPTION(errno) << "Call to getifaddrs() failed";
}

InterfaceAddressesHolder::~InterfaceAddressesHolder()
{
   ::freeifaddrs(m_interfaceAddresses);
}

InterfaceAddressesHolder::operator ifaddrs*() const
{
   return m_interfaceAddresses;
}

} // namespace network
} // namespace cs
