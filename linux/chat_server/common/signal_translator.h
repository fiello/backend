/**
 *  \file
 *  \brief     Helper functionality to extend linux debugging capability
 *  \details   Holds declaration of the SignalTranslator class. It allows assigning
 *             custom handlers for specific linux signals
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_HELPERS_SIGNAL_TRANSLATOR_H
#define CS_HELPERS_SIGNAL_TRANSLATOR_H

#include <logger/logger.h>
#include <execinfo.h>
#include <signal.h>

namespace cs
{
namespace helpers
{

/**
 *  \class     cs::helpers::SignalTranslator
 *  \brief     Implements simple signal handling functionality
 *  \details   Can be useful for capturing backtrace of the application in case
 *             if some critical issue occurred and process has received a specific
 *             signal from OS. For more info see 'man 7 signal'
 */
class SignalTranslator
{
public:
   /**
    * Constructor
    * @param signalId - id of the signal we want to assign a handler for.
    */
   SignalTranslator(int signalId)
   {
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sigemptyset(&sa.sa_mask);
      sa.sa_sigaction = SignalHandler;
      sa.sa_flags = SA_SIGINFO;
      sigaction(signalId, &sa, NULL );
   }

private:
   /// Main signal handler. Signature is tied up to system requirements.
   /// Backtrace is written to standard out/err streams and to the application
   /// log file
   static void SignalHandler(int signal, siginfo_t *si, void *arg)
   {
      const int backtraceBufferSize = 50;
      void *array[backtraceBufferSize];
      int nSize = backtrace(array, backtraceBufferSize);
      char ** symbols = backtrace_symbols(array, nSize);

      LOGEMPTY << "============= Server Backtrace Start =============";
      LOGEMPTY << "Signal number: " << signal << ", processID: " << (int)getppid();
      for (int i = 0; i < nSize; i++)
      {
         LOGEMPTY << symbols[i];
      }
      LOGEMPTY << "============= Server Backtrace End =============";
      abort();
   }
};

/// global SIGSEGV signal handler which will provide detailed backtrace in case
/// if current application will get this signal
static SignalTranslator SegmentationFaultTranslator(SIGSEGV);
/// global SIGFPE signal handler which will provide detailed backtrace in case
/// if current application will get this signal
static SignalTranslator FloatPointTranslator(SIGFPE);

} // namespace helpers
} // namespace cs

#endif // CS_HELPERS_SIGNAL_TRANSLATOR_H
