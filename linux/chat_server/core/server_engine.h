/**
 *  \file
 *  \brief     ServerEngine class declaration
 *  \details   Holds declaration of ServerEngine class (server facade)
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_ENGINE_SERVER_ENGINE_H
#define CS_ENGINE_SERVER_ENGINE_H

#include <network/network_manager.h>
#include <signal/signal_manager.h>
// third-party
#include <boost/noncopyable.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>

namespace cs
{
namespace engine
{

/**
 *  \class     cs::engine::ServerEngine
 *  \brief     Main class which handles all components start up / tear down
 *  \details   Class responsible for creating and launching main server logic, that is:
 *             - signal manager that is responsible for handling shutdown / config update
 *               events
 *             - network manager that is responsible for opening listening ports and
 *               handling all data-flow
 *             - applying config settings based on the signals received from signal manager
 */
class ServerEngine : public boost::noncopyable
{
public:
   /**
    * Constructor. Responsible for creating other managers. Re-throws exception if any was
    * thrown from these managers during their creation.
    */
   ServerEngine();

   /**
    * Destructor. Responsible for launching shutdown procedure. Never throws.
    */
   ~ServerEngine();

   /**
    * Main server routine. Responsible for:
    *  - applying configuration settings
    *  - initializing managers in a proper order
    *  - launching managers and starting signal processing
    * Once all managers are initialized and launched, this function enters blocking mode and
    * will not return unless applications receives some interrupt / terminate signal from OS.
    */
   void Start();

private:
   /// Performs managers and server shutdown in a proper order
   void Shutdown();
   /// Signal handler that is passed to the SignalManager. This functionality is implemented here
   /// because shutdown procedure requires access to all managers. Signal id is the system signal
   /// that should be handled
   void OnSystemSignal(signal::SignalId id);
   /// Apply system-wide configuration settings
   result_t ApplyConfigSettigns();

   /// network manager holder
   boost::scoped_ptr<cs::network::NetworkManager>  m_networkManager;
   /// signal manager holder
   boost::scoped_ptr<signal::SignalManager>        m_signalManager;
   /// flag that shutdown was requested
   bool                                            m_shutdownRequested;
   /// flag that engine has started
   bool                                            m_engineStarted;
};

} // namespace engine
} // namespace cs

/**
 *  \namespace cs::engine
 *  \brief     Holds ServerEngine class and number of data processing tasks
 *  \details   Contains server facade logic (components startup and shutdown) and all the
 *             data processing logic, that is: receiving, parsing, composing answer, executing
 *             chat commands
 */

#endif // CS_ENGINE_SERVER_ENGINE_H
