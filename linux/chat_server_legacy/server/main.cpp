
//native
#include "ExceptionHandler.h"
#include "Logger.h"
#include "ConfigurationModule.h"
#include "IPCModule.h"
#include "CompiledDefinitions.h"
#include "ReturnCodes.h"
//thirdparty
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/mem_fn.hpp>
#include <unistd.h>
#include <errno.h>

//////////////////////////////////////////////////////////////////////////

using namespace Config;
using namespace IPC;

static bool g_bExitProcess = false;


//////////////////////////////////////////////////////////////////////////
/*
 * Signal handler, simply change global variable so that process could exit the main loop and close
 */
static void SignalHandler(int signal,siginfo_t *,void*)
{
	LOGWARN << "Signal received: "<<signal;
	g_bExitProcess = true;
}

//////////////////////////////////////////////////////////////////////////
/*
 * Assign custom handler to some signals which will help exiting the process properly
 */
void WatchSignals()
{
	try
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = SignalHandler;
		sa.sa_flags   = SA_SIGINFO;

		//handle Ctrl+C interrupts
		sigaction( SIGINT, &sa, NULL );
		//safely exit the process in case of any corruption
		sigaction( SIGILL, &sa, NULL );
		//try to avoid abnormal termination
		sigaction( SIGTERM, &sa, NULL );
	}
	CATCH
}

//////////////////////////////////////////////////////////////////////////
/*
 * Core function, construct main objects, start daemon if possible, read config files
 * and parses input options if any.
 */
int main(int argc, char *argv[])
{
	try
	{
		// At the very beginning construct two main singletons, one for acquiring config
		// settings, another is for IPC routines (thread pool, listeners, task scheduling)
		boost::shared_ptr<CConfigurationModule> config;
		boost::shared_ptr<CIPCModule> ipc;
		config.reset(&CConfigurationModule::Instance(),boost::mem_fn(&CConfigurationModule::Destroy));
		ipc.reset(&CIPCModule::Instance(),boost::mem_fn(&CIPCModule::Destroy));

		if (!config->ProcessServerOptions(argc,argv))
		{
			// assuming that some vital options were incorrect or user was printing
			// config information only
			return 0;
		}
		LOGDEBUG << "Configuration file processed: " << CONFIG_FILE;

		// Let's find out if there is another instance of server running. If it is then
		// the only way we can go in this instance is to notify old process about new
		// params or options passed through the command line
		int iDaemonMode = 0;

		// Doesn't matter if the function below fails - just continue
		// working in non-daemon mode then, but log the warning
		if (!config->GetSetting<int>(eCONFIG_DAEMON_MODE,iDaemonMode))
		{
			LOGWARN << "Unable to determine the daemon mode option. "
					<< "Please verify configuration file format is correct";
		}

		if ( (iDaemonMode == 1) && ipc->IsFirstInstance())
		{
			LOGDEBUG << "Launching daemon";
			if (daemon(0,0) < 0)
			{
				LOGERROR << "Unable to launch daemon process, err="<<errno;
				//let's try working as a single process?
			}
		}

		WatchSignals();

		ipc->CreateMessageQueue();
		std::string strIPInterface("");
		int iPort = 0;
		config->GetSetting(eCONFIG_TCP_IF,strIPInterface);
		config->GetSetting(eCONFIG_TCP_PORT,iPort);
		ipc->SetIPSettings(strIPInterface,iPort);
		boost::thread threadListener( boost::bind(&CIPCModule::StartListener,ipc.get(),10));
		boost::thread threadSelector( boost::bind(&CIPCModule::StartSelector,ipc.get()));

		while(!g_bExitProcess)
		{
			sleep(10);
			LOGDEBUG << "Main thread loop";
		}
		LOGDEBUG << "Exiting process";
	}
	CATCH_RETURN(1)
	LOGDEBUG << "Quit.";
	return 0;
}
