/**
 *  \file
 *  \brief     ConnectionManager class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "connection_manager.h"
#include <common/exception_dispatcher.h>
#include <config/configuration_manager.h>
#include <core/data_processing/receive_data_task.h>
#include <core/data_processing/process_message_task.h>

namespace cs
{
namespace network
{

ConnectionManager& ConnectionManager::GetInstance()
{
   // g++ guarantees thread-safe initialization for static variable
   static ConnectionManager manager;
   return manager;
}

ConnectionManager::ConnectionManager()
   : m_epollDescriptor(INVALID_DESCRIPTOR)
   , m_shutdownRequested(false)
   , m_managerIsInitialized(false)
{
   config::ConfigurationManager& configManager = config::ConfigurationManager::GetInstance();

   int poolSize = 0;
   result_t error = configManager.GetSetting(config::FastPoolSize, poolSize);
   if (error != result_code::sOk)
      THROW_BASIC_EXCEPTION(error) << "Unable to create pool for incoming tasks";

   m_fastPool.reset( new thread_pool::ThreadPool(poolSize) );

   poolSize = 0;
   error = configManager.GetSetting(config::SlowPoolSize, poolSize);
   if (error != result_code::sOk)
      THROW_BASIC_EXCEPTION(error) << "Unable to create pool for outgoing tasks";

   m_slowPool.reset( new thread_pool::ThreadPool(poolSize) );
}

ConnectionManager::~ConnectionManager()
{
   if (m_epollDescriptor == INVALID_DESCRIPTOR)
   {
      LOGERR << "Close attempt on invalid epoll descriptor";
      return;
   }

   if (::close(m_epollDescriptor) != 0)
   {
      LOGERR << "Error while closing epoll descriptor, system error message: " << strerror(errno);
   }

   m_epollDescriptor = INVALID_DESCRIPTOR;
}

void ConnectionManager::Initialize(const unsigned int backlogSize)
{
   if (m_managerIsInitialized)
      THROW_BASIC_EXCEPTION(result_code::eUnexpected) << "Connection manager is already initialized!";

   m_managerIsInitialized = true;
   m_fastPool->Initialize();
   m_slowPool->Initialize();

   EpollDescriptor descriptor = ::epoll_create(backlogSize);
   if (descriptor == INVALID_DESCRIPTOR)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to create epoll object";

   m_epollDescriptor = descriptor;
}

void ConnectionManager::Shutdown()
{
   if (!m_shutdownRequested && m_managerIsInitialized)
   {
      m_shutdownRequested = true;
      m_managerIsInitialized = false;
      LOGDBG << "Shutdown ConnectionManager";
      m_fastPool->Shutdown();
      m_slowPool->Shutdown();

      // carefully close each of the remained connections
      SocketDescriptor socket;
      LOCK lock(m_activeConnectionAccessGuard);
      for (ConnectionStorage::const_iterator it = m_activeConnections.begin();
         it != m_activeConnections.end();
         ++it)
      {
         socket = it->first;
         LOGWRN << "Deleting remaining socket: " << socket;
         int error = ::epoll_ctl(m_epollDescriptor, EPOLL_CTL_DEL, socket, 0);
         if (error != 0)
         {
            LOGWRN << "Error deleting socket " << socket
                  << " from epoll, system error message: " << ::strerror(errno);
         }
      }
      m_activeConnections.clear();
   }
}

void ConnectionManager::ProcessConnections(const int timeout)
{
   int epollResult = ::epoll_wait(m_epollDescriptor, m_epollEvents, MaxEpollEventsCount, timeout);

   // if shutdown was requested then exit immediately without processing any events
   if (m_shutdownRequested)
   {
      LOGDBG << "Emergence exit was requested, skip events handling";
      return;
   }

   if (epollResult == INVALID_DESCRIPTOR)
      THROW_NETWORK_EXCEPTION(errno) << "Failed to wait on incoming connection";

   // traverse through triggered events and process them one by one
   ConnectionHolderPtr triggeredConnection;
   ConnectionCarrier* carrier;
   for (int i = 0; i < epollResult; ++i)
   {
      if (m_epollEvents[i].events & EPOLLERR)
      {
         LOGERR << "TCP/IP stack error";
      }
      else
      {
         carrier = (ConnectionCarrier*)m_epollEvents[i].data.ptr;
         triggeredConnection = carrier->holder.lock();
         if (triggeredConnection.get())
         {
            OnConnectionEvent(triggeredConnection);
         }
      }
   }
   triggeredConnection.reset();

   ApplyDeleteList();
}

void ConnectionManager::AddConnection(const ConnectionHolderPtr connectionHolder)
{
   CHECK_ARGUMENT(connectionHolder.get() != 0, "Empty connection!");
   CHECK_ARGUMENT(connectionHolder->IsSocketValid(), "Socket is invalid");

   // force adding new connections with Edge Triggered mode. Listening sockets
   // remain in default mode (Level Triggered)
   uint32_t edgeTriggeredFlag = 0;
   if (!connectionHolder->IsListeningSocket())
      edgeTriggeredFlag |= EPOLLET;

   SocketDescriptor socket = connectionHolder->GetSocketDescriptor();
   ConnectionCarrierPtr carrier( new ConnectionCarrier );
   connectionHolder->SetConnectionCarrier(carrier);
   carrier->holder = connectionHolder;

   epoll_event event;
   event.events = EPOLLIN | EPOLLERR | edgeTriggeredFlag;
   event.data.ptr = carrier.get();

   int error = ::epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, socket, &event);
   if (error != 0)
      THROW_NETWORK_EXCEPTION(errno) << "Unable to add new descriptor to the epoll object";

   LOCK lock(m_activeConnectionAccessGuard);
   // Force erasing old connection if any. Otherwise we could face race condition and
   // unable to insert newly opened connection
   m_activeConnections.erase(socket);
   m_activeConnections.insert(std::make_pair(socket, connectionHolder));
}

void ConnectionManager::RemoveConnection(const SocketDescriptor socket)
{
   LOGDBG << "Add pending removal for socket " << socket;

   LOCK lock(m_pendingConnectionsAccessGuard);
   m_pendingConnectionsToDelete.push_back(socket);
}

void ConnectionManager::PostFastTask(engine::TaskPtr task)
{
   if (!m_shutdownRequested)
   {
      thread_pool::ThreadTask taskFunctor = boost::bind(&engine::ITask::Execute, task);
      m_fastPool->AddTask(taskFunctor);
   }
}

void ConnectionManager::PostSlowTask(engine::TaskPtr task)
{
   if (!m_shutdownRequested)
   {
      thread_pool::ThreadTask taskFunctor = boost::bind(&engine::ITask::Execute, task);
      m_slowPool->AddTask(taskFunctor);
   }
}

void ConnectionManager::GetActiveConnections(ConnectionHolderList& activeConnections)
{
   ConnectionHolderList tempList;

   {
      LOCK lock(m_activeConnectionAccessGuard);
      for (ConnectionStorage::const_iterator it = m_activeConnections.begin();
         it != m_activeConnections.end();
         ++it)
      {
         tempList.push_back(it->second);
      }
   }

   activeConnections.swap(tempList);
}

result_t ConnectionManager::FindConnectionByUsername(const std::string& username, ConnectionHolderPtr& connectionHolder)
{
   CHECK_ARGUMENT(!username.empty(), "Username should not be empty!");

   LOCK lock(m_activeConnectionAccessGuard);
   for (ConnectionStorage::const_iterator it = m_activeConnections.begin();
      it != m_activeConnections.end();
      ++it)
   {
      if (it->second->GetUsername() == username)
      {
         connectionHolder = it->second;
         return result_code::sOk;
      }
   }

   return result_code::eNotFound;
}

result_t ConnectionManager::SetClientUsername(const SocketDescriptor sourceSocket, const std::string& username)
{
   CHECK_ARGUMENT(sourceSocket != INVALID_DESCRIPTOR, "Invalid socket descriptor!");

   ConnectionHolderPtr sourceConnection;

   LOCK lock(m_activeConnectionAccessGuard);
   for (ConnectionStorage::const_iterator it = m_activeConnections.begin();
      it != m_activeConnections.end();
      ++it)
   {
      if (it->second->GetUsername() == username)
         return result_code::eAlreadyDefined;

      if (it->first == sourceSocket)
         sourceConnection = it->second;
   }

   if (!sourceConnection.get())
      return result_code::eNotFound;

   sourceConnection->SetUsername(username);
   return result_code::sOk;
}

void ConnectionManager::OnConnectionEvent(ConnectionHolderPtr triggeredConnection)
{
   try
   {
      CHECK_ARGUMENT(triggeredConnection.get() != 0, "Invalid incoming connection!");
      CHECK_ARGUMENT(triggeredConnection->IsSocketValid(), "Invalid socket descriptor");

      if (triggeredConnection->IsListeningSocket())
      {
         // accept connection from new client and add it to co nnection listener
         SocketAddressHolder newSocketAddress;
         SocketDescriptor socket = triggeredConnection->AcceptNewConnection(newSocketAddress);
         LOGDBG << "New connect on socket " << socket;
         SocketWrapperPtr newSocket( new SocketWrapper(socket) );
         newSocket->SetNonblocking();
         // TCP_NODELAY option will help us to achieve lower latency on little
         // portions of data to be sent out
         newSocket->SetSocketOption(SOL_TCP, TCP_NODELAY, 1);
         // set keep-alive option as we are working with connection-oriented socket
         newSocket->SetSocketOption(SOL_TCP, SO_KEEPALIVE, 1);

         // mark this connection holder with 'false' flag as it's not a listening
         // socket but just a new connection
         ConnectionHolderPtr newConnectionHolder( new ConnectionHolder(newSocket, false) );
         newConnectionHolder->SetUsername();
         AddConnection(newConnectionHolder);

         // post message to notify that a new user has joined
         engine::MessageDescription message;
         message.receiver = newConnectionHolder;
         message.senderSocket = socket;
         message.senderName = engine::ServerSenderName;
         message.data = "User '" + newConnectionHolder->GetUsername() +
               "' has joined the chat" + engine::ChatTerminationSymbol;
         engine::TaskPtr newTask( new engine::ProcessMessageTask(message) );
         PostSlowTask(newTask);

         // post intro message to the newbie message
         message.data = std::string("\\intro") + engine::ChatTerminationSymbol;
         engine::TaskPtr introTask( new engine::ProcessMessageTask(message) );
         PostSlowTask(introTask);
      }
      else
      {
         // launch read task on existing socket
         LOGDBG << "Launch read on socket: " << triggeredConnection->GetSocketDescriptor();
         engine::TaskPtr newTask( new engine::ReceiveDataTask(triggeredConnection) );
         PostFastTask(newTask);
      }
   }
   catch(const std::exception&)
   {
      helpers::ExceptionDispatcher::Dispatch(BOOST_CURRENT_FUNCTION);
   }
}

void ConnectionManager::ApplyDeleteList()
{
   LOCK lockPending(m_pendingConnectionsAccessGuard);
   if (!m_pendingConnectionsToDelete.size())
      return;

   LOCK lockActive(m_activeConnectionAccessGuard);
   for (SocketList::const_iterator it = m_pendingConnectionsToDelete.begin();
      it != m_pendingConnectionsToDelete.end();
      ++it)
   {
      // According to the system documentation socket closure causes descriptor to be
      // erased from epoll set automatically. But we better force manual erase to keep
      // epoll up-to-date
      ::epoll_ctl(m_epollDescriptor, EPOLL_CTL_DEL, *it, 0);
      ConnectionStorage::iterator connectionPair = m_activeConnections.find(*it);
      if (connectionPair != m_activeConnections.end() &&
          connectionPair->second->IsConnectionClosed())
      {
         // It's also a possible case due to race condition - one thread is reading from
         // socket while another was awaken by ConnectionManager on new epoll event (which
         // is actually the 'on-close' edge). As a result two threads are running independently,
         // both are notified about socket closure and post socket descriptor to pending list.
         m_activeConnections.erase(connectionPair);
      }
   }
   m_pendingConnectionsToDelete.clear();
}

} // namespace network
} // namespace cs
