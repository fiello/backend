#ifndef CS_SIGNAL_SIGNAL_MANAGER_H
#define CS_SIGNAL_SIGNAL_MANAGER_H

#include <signal.h>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

namespace cs
{
namespace signal
{

typedef int SignalId;
typedef boost::function<void(const SignalId id)> SignalHandler;

class SignalManager : public boost::noncopyable
{
public:
   SignalManager();
   void Initialize(const long signalWaitTimeout, const SignalHandler externalHandler);
   void ProcessSignals();
   void Shutdown();

private:
   bool m_shutdownRequested;
   bool m_managerStarted;
   SignalHandler m_handler;
   long m_signalWaitTimeout;
   sigset_t m_signalSet;
};

} // namespace signal
} // namespace cs

#endif // CS_SIGNAL_SIGNAL_MANAGER_H
