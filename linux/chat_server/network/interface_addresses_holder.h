/**
 *  \file
 *  \brief     InterfaceAddressesHolder class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_INTERFACE_ADDRESSES_HOLDER_H
#define CS_NETWORK_INTERFACE_ADDRESSES_HOLDER_H

// third-party
#include <ifaddrs.h>
#include <boost/noncopyable.hpp>

namespace cs
{
namespace network
{

/**
 *  \class     cs::network::InterfaceAddressesHolder
 *  \brief     Holds interface address (linux specific)
 *  \details   Implements RAII pattern for 'ifaddrs' structure in order to create and
 *             release it properly.
 */
class InterfaceAddressesHolder
{
public:
   /**
    * Constructor, responsible for structure creation
    */
   InterfaceAddressesHolder();

   /**
    * Destructor, responsible for structure release
    */
   ~InterfaceAddressesHolder();

   /**
    * Overloaded operator, in order to get access to internal structure
    * that describes network interface
    */
   operator ifaddrs*() const;

private:
   /// Structure that describes network interface
   ifaddrs* m_interfaceAddresses;
};

} // namespace network
} // namespace cs

#endif // CS_NETWORK_INTERFACE_ADDRESS_HOLDER_H
