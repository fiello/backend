/**
 *  \file
 *  \brief     Define common types to work with descriptors in cs::network namespace
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_DESCRIPTOR_H
#define CS_NETWORK_DESCRIPTOR_H

namespace cs
{
namespace network
{

/// type of descriptor for epoll object
typedef int EpollDescriptor;
/// type of descriptor for socket
typedef int SocketDescriptor;
/// value of invalid descriptor (linux specific)
static const int INVALID_DESCRIPTOR = -1;

} // namespace network
} // namespace cs

#endif // CS_NETWORK_DESCRIPTOR_H
