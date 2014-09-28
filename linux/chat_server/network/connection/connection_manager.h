/**
 *  \file
 *  \brief     ConnectionManager class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_CONNECTION_MANAGER_H
#define CS_NETWORK_CONNECTION_MANAGER_H

#include "connection_holder.h"
#include <common/result_code.h>
#include <thread_pool/thread_pool.h>
// third-party
#include <sys/epoll.h>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <map>
#include <list>

namespace cs
{
namespace network
{

/// type of main connection container - each socket has one connection associated with it
typedef std::map<SocketDescriptor, ConnectionHolderPtr> ConnectionStorage;
/// type of container to pass list of connections between components
typedef std::list<ConnectionHolderPtr> ConnectionHolderList;
/// type of container to be used for socket list
typedef std::list<SocketDescriptor> SocketList;

/// size of the array to handle active connection events
static const int MaxEpollEventsCount = 4096;

/**
 *  \class     cs::network::ConnectionManager
 *  \brief     Main class that handles all incoming/outgoing network activity
 *  \details   General class that holds list of all active connections, performs
 *             connection processing (open, accept, listen, close), contains two thread
 *             pools for better connection processing: front-end pool for fast tasks
 *             (read/write) and backend pool for slow tasks (parse data, execute
 *             command, maintain list of connections.
 *             Object implemented as a singleton and can be accessed from other
 *             parts of application.
 */
class ConnectionManager : public boost::noncopyable
{
public:

   /**
    * Method to get access to singleton object
    * @returns - reference to current instance of ConnectionManager object
    */
   static ConnectionManager& GetInstance();

   /**
    * Destructor, clears internal manager resources
    */
   ~ConnectionManager();

   /**
    * Initializes manager resources: epoll kernel object, thread pools
    * @param backlogSize - size of the backlog connections, required by epoll object
    */
   void Initialize(const unsigned int backlogSize = 100 /* argument is unused since Linux 2.6.8 */);

   /**
    * Shutdown procedure. Closes thread pools, removes active connections from epoll
    * object, closes all active connections
    */
   void Shutdown();

   /**
    * Method to be used by external caller (NetworkManager) to process connections periodically.
    * This method is blocked for certain timeout during which it is waiting for incoming network
    * activity. On new network event it awakes, schedules new connections to be processed and
    * return control to the caller.
    * @param timeout - time period to wait for incoming network activity
    */
   void ProcessConnections(const int timeout);

   /**
    * Add new connection to the list of active connections and to the epoll kernel object. From now
    * on all events fired from this connection will be handled by ConnectionManager. Current
    * implementation adds all new connections with Level Triggered mode except for the case when it
    * is a listening connection - this one goes with Edge Triggered mode.
    * @param connectionHolder - smart object that holds new incoming connection to be added
    */
   void AddConnection(const ConnectionHolderPtr connectionHolder);

   /**
    * Mark appropriate connection for closure using socket descriptor number. Socket is placed to the
    * pending list of connections that should be closed. The reason it is not closed here is that epoll
    * object stores link to this connection object. If we delete connection here then the one in epoll
    * object will become invalid which is not good.
    * @param socket - socket of the connection to be added to pending list for removal/closure
    */
   void RemoveConnection(const SocketDescriptor socket);

   /**
    * Post task to the front-end (fast) pool
    * @param task - smart object that holds task to be added to the fast pool
    */
   void PostFastTask(engine::TaskPtr task);

   /**
    * Post task to the backend-end (slow) pool
    * @param task - smart object that holds task to be added to the slow pool
    */
   void PostSlowTask(engine::TaskPtr task);

   /**
    * Post task to the backend-end (slow) pool
    * @param activeConnections - reference to the list that will be fulfilled with all active
    *                            connections opened at the moment
    */
   void GetActiveConnections(ConnectionHolderList& activeConnections);

   /**
    * Find for active connection by given username.
    * @param username - reference to the string with username to be found
    * @param connectionHolder - smart object that will be holding found connection if any matched
    *                           the given username
    * @returns - result code of the operation:
    *             - sOk if connection was found
    *             - eNotFound if no connection was found
    */
   result_t FindConnectionByUsername(const std::string& username, ConnectionHolderPtr& connectionHolder);

   /**
    * Associate given username with the specific socket
    * @param sourceSocket - socket descriptor to assign a name to
    * @param username - reference to the string with the username to be assigned
    */
   result_t SetClientUsername(const SocketDescriptor sourceSocket, const std::string& username);

private:
   /// type for commonly used lock object
   typedef boost::lock_guard<boost::mutex> LOCK;

   /// restrict default constructor to meet singleton pattern
   ConnectionManager();
   /// Main method to handle connection event. Schedules a new read task if it's an old connection
   /// and establishes a new connection if we got event from listening socket.
   void OnConnectionEvent(ConnectionHolderPtr triggeredConnection);
   /// Helper method to be called at the end of ProcessConnections routine to erase all pending
   /// connections
   void ApplyDeleteList();

   /// smart object that holds fast thread pool
   boost::scoped_ptr<thread_pool::ThreadPool>   m_fastPool;
   /// smart object that holds slow thread pool
   boost::scoped_ptr<thread_pool::ThreadPool>   m_slowPool;

   /// sync object to guard access to list of active connections
   boost::mutex                                 m_activeConnectionAccessGuard;
   /// container that holds active connections and corresponding socket descriptors
   ConnectionStorage                            m_activeConnections;
   /// sync object to guard access to pending list of connections to be closed
   boost::mutex                                 m_pendingConnectionsAccessGuard;
   /// pending list of connections to be closed
   SocketList                                   m_pendingConnectionsToDelete;

   /// descriptor of the epoll kernel object
   EpollDescriptor                              m_epollDescriptor;
   /// flag that shutdown was requested
   bool                                         m_shutdownRequested;
   /// flag that manager is initialized already
   bool                                         m_managerIsInitialized;
   /// array where new events will be copied upon the trigger from epoll
   epoll_event                                  m_epollEvents[MaxEpollEventsCount];
};

} // namespace network
} // namespace cs

#endif // CS_NETWORK_CONNECTION_MANAGER_H
