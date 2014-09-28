/**
 *  \file
 *  \brief     ServerEngine class implementation
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#include "server_engine.h"
#include <config/configuration_manager.h>
#include <common/exception_dispatcher.h>
// third-party
#include <unistd.h>
#include <boost/bind.hpp>

namespace cs
{
namespace engine
{

ServerEngine::ServerEngine()
   : m_shutdownRequested(false)
   , m_engineStarted(false)
{
   try
   {
      m_networkManager.reset( new network::NetworkManager() );
      m_signalManager.reset( new signal::SignalManager() );
   }
   catch(const std::exception &ex)
   {
      LOGFTL << "Unexpected exception, error message: " << ex.what();
      throw;
   }
}

ServerEngine::~ServerEngine()
{
   try
   {
      Shutdown();
   }
   catch(const std::exception &ex)
   {
      // log exception but don't allow its propagation further
      LOGFTL << "Unexpected exception, error message: " << ex.what();
   }
}

void ServerEngine::Start()
{
   try
   {
      if (m_engineStarted)
         THROW_BASIC_EXCEPTION(result_code::eUnexpected) << "ServerEngine has already started";

      result_t error = ApplyConfigSettigns();
      if (error != result_code::sOk)
         THROW_BASIC_EXCEPTION(error) << "Unable to start server engine, configuration settings are incorrect";
      m_engineStarted = true;

      // Initialize SignalManager
      // Need to do it here, before any child thread starts. In this case every child thread
      // will inherit the same signal mask and SignalManager will be able handle signals properly
      signal::SignalHandler handler = boost::bind(&ServerEngine::OnSystemSignal, this, _1);
      const long signalWaitTimeout = 300;
      m_signalManager->Initialize(signalWaitTimeout, handler);

      // Initialize NetworkManager
      m_networkManager->Initialize();
      m_networkManager->Start();
      m_signalManager->ProcessSignals();
   }
   catch(const std::exception& ex)
   {
      LOGERR << "Unexpected exception: " << ex.what();
      throw;
   }
}

void ServerEngine::Shutdown()
{
   if (!m_shutdownRequested && m_engineStarted)
   {
      LOGDBG << "Start server shutdown procedure";
      m_shutdownRequested = true;
      m_networkManager->Shutdown();
      m_signalManager->Shutdown();
      m_engineStarted = false;
   }
}

void ServerEngine::OnSystemSignal(signal::SignalId id)
{
   LOGDBG << "Signal thread handling new signal, id: " << id;
   switch (id)
   {
      case SIGTERM:
      case SIGINT:
      case SIGKILL:
         Shutdown();
         break;
      case SIGHUP:
      {
         LOGDBG << "Reload configuration settings from file";
         config::ConfigurationManager& configManager = config::ConfigurationManager::GetInstance();
         configManager.LoadSettingsFromFile();
         ApplyConfigSettigns();
         break;
      }
      default:
         break;
   }
}

result_t ServerEngine::ApplyConfigSettigns()
{
   config::ConfigurationManager& configManager = config::ConfigurationManager::GetInstance();
   int tempValue = 0;

   // daemon
   result_t error = configManager.GetSetting(config::Daemon, tempValue);
   if (error != result_code::sOk)
      return error;
   if (tempValue)
      ::daemon(/* don't change dir*/ 1, /*reroute stdin/out/err to /dev/null */ 0);

   // log level
   error = configManager.GetSetting(config::LogLevel, tempValue);
   if (error != result_code::sOk)
      return error;
   SET_LOG_LEVEL(tempValue);

   return result_code::sOk;
}


} // namespace engine
} // namespace cs
