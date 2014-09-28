/**
 *  \file
 *  \brief     ConnectionHolder class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "connection_holder.h"
#include <common/exception_dispatcher.h>
#include <network/connection/connection_manager.h>
#include <core/data_processing/process_message_task.h>

namespace cs
{
namespace network
{

static const size_t MaxDataBufferSize = 1024;
static const size_t MaximumMessageLength = 8192;

ConnectionHolder::ConnectionHolder(const SocketWrapperPtr socket, const bool isListeningSocket)
   : m_isListeningSocket(isListeningSocket)
   , m_isConnectionClosed(false)
{
   CHECK_ARGUMENT(socket.get(), "Empty socket!");
   CHECK_ARGUMENT(socket->IsValid(), "Inavlid socket!");
   m_socketWrapper = socket;
}

ConnectionHolder::~ConnectionHolder()
{
   engine::MessageDescription message;
   message.senderName = engine::ServerSenderName;
   message.senderSocket = m_socketWrapper->GetDescriptor();
   message.data = std::string("User '") + GetUsername() + "' has left the chat " + engine::ChatTerminationSymbol;
   engine::TaskPtr newTask( new engine::ProcessMessageTask(message) );
   ConnectionManager::GetInstance().PostSlowTask(newTask);
}

bool ConnectionHolder::IsListeningSocket() const
{
   return m_isListeningSocket;
}

bool ConnectionHolder::IsConnectionClosed() const
{
   return m_isConnectionClosed;
}

result_t ConnectionHolder::ReadAndAppendSocketData()
{
   try
   {
      char dataBuffer[MaxDataBufferSize] = {0};
      ssize_t readResult = 0;
      std::string tempData;

      LOCK lock(m_socketDataAccessGuard);
      do
      {
         readResult = m_socketWrapper->Read(dataBuffer, MaxDataBufferSize);
         tempData.append(dataBuffer);
         ::memset(&dataBuffer, 0, MaxDataBufferSize);
      }
      while (readResult > 0);

      size_t sumLength = tempData.length() + m_socketData.length();
      if (sumLength >= MaximumMessageLength)
      {
         LOGERR << "Message length is exceeded on socket: " << m_socketWrapper->GetDescriptor();
         return result_code::eBufferOverflow;
      }
      m_socketData.append(tempData);

      if (readResult == 0)
         return result_code::eConnectionClosed;
      else
         return result_code::sOk;
   }
   catch(const std::exception&)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

result_t ConnectionHolder::GetNextSocketData(std::string& tempString)
{
   try
   {
      LOCK lock(m_socketDataAccessGuard);
      size_t terminationPosition = m_socketData.rfind(engine::ChatTerminationSymbol);
      if (terminationPosition == std::string::npos)
         return result_code::eNotFound;

      ++terminationPosition; // increment by one to capture '\n' symbol as well

      tempString.assign(m_socketData.substr(0, terminationPosition));
      m_socketData.erase(0, terminationPosition);
      return result_code::sOk;
   }
   catch(const std::exception&)
   {
      return helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

void ConnectionHolder::SetUsername(const std::string& newUsername)
{
   if (!newUsername.empty())
   {
      m_username = newUsername;
      return;
   }

   //  if newUsername is empty - simply assign a random one
   static long userId = 0;
   time_t rawTime;
   ::time(&rawTime);
   std::ostringstream out;
   out << "user_" << rawTime << "_" << ++userId;
   m_username = out.str();
}

std::string ConnectionHolder::GetUsername() const
{
   return m_username;
}

void ConnectionHolder::Close()
{
   m_isConnectionClosed = true;
   ConnectionManager::GetInstance().RemoveConnection(m_socketWrapper->GetDescriptor());
   m_socketWrapper->Close();
}

void ConnectionHolder::SetConnectionCarrier(ConnectionCarrierPtr carrier)
{
   // Trick to maintain carrier life-span by the connection it holds, consists of 3 steps:
   // 1) Each ConnectionHolderPtr spawned from the given holds shared_ptr with the carrier
   // 2) Upon destruction of the last connection (ApplyDeleteList method or in ThreadTask) carrier will be
   // disposed automatically in destructor
   // 3) Epoll object will no longer hold this particular carrier because socket will
   // be closed and its descriptor will be erased from epoll set by the system
   m_carrier = carrier;
}

bool ConnectionHolder::IsSocketValid() const
{
   return m_socketWrapper->IsValid();
}

SocketDescriptor ConnectionHolder::GetSocketDescriptor() const
{
   return m_socketWrapper->GetDescriptor();
}

SocketDescriptor ConnectionHolder::AcceptNewConnection(SocketAddressHolder& socketAddress)
{
   return m_socketWrapper->Accept(socketAddress);
}

ssize_t ConnectionHolder::WriteDataToSocket(const std::string& dataBuffer)
{
   return m_socketWrapper->Write(dataBuffer);
}


} // namespace network
} // namespace cs
