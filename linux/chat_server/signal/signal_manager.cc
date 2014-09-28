#include "signal_manager.h"
#include <logger/logger.h>
// third-party
#include <execinfo.h>

namespace cs
{
namespace signal
{

SignalManager::SignalManager()
   : m_shutdownRequested(false)
   , m_managerStarted(false)
   , m_signalWaitTimeout(0)
{}

void SignalManager::Initialize(const long signalWaitTimeout, const SignalHandler externalHandler)
{
   LOGDBG << "Initializing SignalManager";
   ::sigfillset(&m_signalSet);
   ::sigdelset(&m_signalSet, SIGSEGV);
   ::sigdelset(&m_signalSet, SIGFPE);
   ::pthread_sigmask(SIG_BLOCK, &m_signalSet, 0);

   m_handler = externalHandler;
   m_signalWaitTimeout = signalWaitTimeout;
}

void SignalManager::ProcessSignals()
{
   LOGDBG << "Starting SignalManager";
   m_managerStarted = true;
   timespec signalTimeout;
   signalTimeout.tv_sec = 0;	// we don't need seconds here, it's a too long delay
   signalTimeout.tv_nsec = m_signalWaitTimeout*1000000;	// convert from milisec to nanosec
   siginfo_t signalInfo;

   while(!m_shutdownRequested)
   {
      if (::sigtimedwait(&m_signalSet, &signalInfo, &signalTimeout) > 0)
         m_handler(signalInfo.si_signo);
   }
}

void SignalManager::Shutdown()
{
   if (m_managerStarted && !m_shutdownRequested)
   {
      LOGDBG << "Shutdown SignalManager";
      m_shutdownRequested = true;
   }
}

} // namespace signal
} // namespace cs
