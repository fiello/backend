/**
 *  \file
 *  \brief     SocketWrapper class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "socket_wrapper.h"
#include <common/exception_dispatcher.h>
// third-party
#include <unistd.h>
#include <fcntl.h>

namespace cs
{
namespace network
{

SocketWrapper::SocketWrapper(const SocketDescriptor socket)
   : m_isClosed(false)
{
   if (m_socket == INVALID_DESCRIPTOR)
      THROW_INVALID_ARGUMENT;

   m_socket = socket;
}

SocketWrapper::SocketWrapper(const int domain, const int type, const int protocol)
   : m_isClosed(false)
{
   SocketDescriptor tempSocket = ::socket(domain, type, protocol);
   if (tempSocket == INVALID_DESCRIPTOR)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to create socket";

   m_socket = tempSocket;
   LOGDBG << "Opened socket: " << m_socket;
}

SocketWrapper::~SocketWrapper()
{
   Close();
}

SocketDescriptor SocketWrapper::GetDescriptor() const
{
   return m_socket;
}

void SocketWrapper::SetSocketOption(const int level, const int optionName, const int optionValue)
{
   int error = ::setsockopt(m_socket, level, optionName, &optionValue, sizeof(optionValue));
   if (error != 0)
      THROW_NETWORK_EXCEPTION(error) << "Unable to setup socket option";
}

void SocketWrapper::SetNonblocking()
{
   int value = 1;
   int error = ::fcntl(m_socket, F_SETFL, O_NONBLOCK, value, sizeof(value));
   if (error == INVALID_DESCRIPTOR)
   {
      LOGERR << "Error while setting nonblocking option for the socket";
   }
}

SocketDescriptor SocketWrapper::Accept(SocketAddressHolder& socketAddress)
{
   sockaddr remoteAddress;
   socklen_t length;
   SocketDescriptor result = ::accept(m_socket, &remoteAddress, &length);
   if (result == INVALID_DESCRIPTOR)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to accept new incoming connection";
   socketAddress = remoteAddress;
   return result;
}

void SocketWrapper::Bind(const SocketAddressHolder& address)
{
   int error = ::bind(m_socket, (const sockaddr*)address, address.GetSize());
   if (error != 0)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to bind socket address";
}

void SocketWrapper::Listen(const int backlogSize)
{
   int error = ::listen(m_socket, backlogSize);
   if (error != 0)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to set socket listening";
}

ssize_t SocketWrapper::Read(void *dataBuffer, const size_t bytesCount)
{
	ssize_t readResult = ::read(m_socket, dataBuffer, bytesCount);
	if (readResult > 0)
	{
		// force zero symbol at the end of data to get rid of possible garbage in socket
		((char*)dataBuffer)[readResult] = 0;
	}
	else if (readResult == -1)
	{
		if (errno != EAGAIN)
			THROW_NETWORK_EXCEPTION(errno) << "Unable to read from socket: " << m_socket;
	}
	return readResult;
}

ssize_t SocketWrapper::Write(const std::string& dataBuffer)
{
   size_t bytesCount = dataBuffer.length();
   ssize_t writeResult = ::send(m_socket, dataBuffer.c_str(), bytesCount, 0);
   return writeResult;
}

// Don't permit throwing in this function as it is executed in destructor / shutdown procedure.
// Interrupting the shutdown procedure might end up in resource leak.
void SocketWrapper::Close()
{
   if (m_isClosed)
      return;

   m_isClosed = true;
   LOGDBG << "Closing socket: " << m_socket;

   if (m_socket == INVALID_DESCRIPTOR)
   {
      LOGERR << "Close attempt on invalid socket descriptor";
      return;
   }

   if (::close(m_socket) != 0)
   {
      LOGERR << "Error while closing socket descriptor, system error message: " << strerror(errno);
   }

   m_socket = INVALID_DESCRIPTOR;
}

bool SocketWrapper::IsValid() const
{
   return !m_isClosed && (m_socket != INVALID_DESCRIPTOR);
}


} // namespace network
} // namespace cs
