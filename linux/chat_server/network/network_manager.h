/**
 *  \file
 *  \brief     NetworkManager class declaration
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_NETWORK_NETWORK_MANAGER_H
#define CS_NETWORK_NETWORK_MANAGER_H

#include <common/result_code.h>
#include <network/connection/connection_manager.h>
// third-party
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>

namespace cs
{
namespace network
{

/**
 * \class   cs::network::NetworkManager
 * \brief   Main class responsible for network start and shutdown
 * \details Management class responsible for network components initialize/start up/tear down
 *          procedures. Validates network settings during initialization, starts
 *          listening threads
 */
class NetworkManager : public boost::noncopyable
{
public:
   /**
    * Constructor
    */
   NetworkManager();

   /**
    * Initialization routine, responsible for validating network configuration
    * settings, opening first listening connection.
    */
   void Initialize();

   /**
    * Startup routine, launches listening thread which will be responsible for accepting
    * new connections.
    */
   void Start();

   /**
    * Shutdown procedure, performs listening thread and the whole component shutdown
    */
   void Shutdown();

private:
   /// Routine for the thread that listens to new incoming connections
   void ListeningThreadRoutine();

   /// flag that shutdown is requested and component must shutdown
   bool                             m_shutdownRequested;
   /// wrapper for listener thread
   boost::scoped_ptr<boost::thread> m_listenerThread;
};

} // namespace network
} // namespace cs

/**
 * \namespace  cs::network
 * \brief      Holds all the network related functionality.
 * \details    Holds all functionality responsible for interacting with network, that is:
 *             Network manager to start networking, ConnectionManager that handles all active
 *             connections, SocketWrapper to work with sockets smoothly, ConnectionHolder that
 *             wraps socket with some helper flags into one entity known as 'connection'.
 *             Processing thread pools are also located here.
 */

#endif // CS_NETWORK_NETWORK_MANAGER_H
