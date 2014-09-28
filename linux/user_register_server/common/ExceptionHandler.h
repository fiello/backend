
#ifndef EXCEPTIONHANDLER_H_
#define EXCEPTIONHANDLER_H_

//native
#include "Logger.h"
//thirdparty
#include <execinfo.h>
#include <signal.h>
#include <iostream>

//////////////////////////////////////////////////////////////////////////

#define BACKTRACE_BUF_SIZE 50

//////////////////////////////////////////////////////////////////////////
/*
 * Auxiliary class to provide some kind of backtrace (if possible) in case of process crash.
 * Intended to handle SIGFPE and SIGSEGV signals
 */
template <class TExceptionClass> class CSignalTranslator
{
private:
	class SingletonTranslator
	{
	public:
		SingletonTranslator()
		{
			struct sigaction sa;
			memset(&sa, 0, sizeof(sa));
			sigemptyset(&sa.sa_mask);
			sa.sa_sigaction = SignalHandler;
			sa.sa_flags   = SA_SIGINFO;
			sigaction( TExceptionClass::GetSignalNumber(), &sa, NULL );
		}

		static void SignalHandler(int signal, siginfo_t *si, void *arg)
		{
			void *array[BACKTRACE_BUF_SIZE];
			int nSize = backtrace(array, BACKTRACE_BUF_SIZE);
			char ** symbols = backtrace_symbols(array, nSize);

			LOGFATAL_SAFE << "============= Server Backtrace Start =============";
			LOGFATAL_SAFE << "Signal number: " << signal << ", processID: " << (int)getppid();
			for (int i = 0; i < nSize; i++)
			{
				LOGFATAL_SAFE << symbols[i];
			}
			LOGFATAL_SAFE << "============= Server Backtrace End =============";
			abort();
		}
	};

public:
	CSignalTranslator()
	{
		static SingletonTranslator s_objTranslator;
	}
};

//////////////////////////////////////////////////////////////////////////
class CSegmentationFault
{
public:
	static int GetSignalNumber() { return SIGSEGV; }
};

static CSignalTranslator<CSegmentationFault> g_objSegFaultTranslator;

//////////////////////////////////////////////////////////////////////////
class CFloatingPointException
{
public:
	static int GetSignalNumber() { return SIGFPE; }
};

static CSignalTranslator<CFloatingPointException> g_objFloatPointTranslator;

//////////////////////////////////////////////////////////////////////////

#endif //#ifndef EXCEPTIONHANDLER_H_
