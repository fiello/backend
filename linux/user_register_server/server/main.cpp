
//native
#include "ExceptionHandler.h"
#include "ConfigurationModule.h"
#include "IPCModule.h"
//thirdparty
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/mem_fn.hpp>
#include <errno.h>

//////////////////////////////////////////////////////////////////////////

using namespace Config;
using namespace IPC;


//////////////////////////////////////////////////////////////////////////
/*
 * Signal handler, simply trigger IPC conditional variable with the known name so that the process can
 * finish running properly
 * @param signal - triggered signal integer ID
 */
static void SignalHandler(int signal,siginfo_t *,void*)
{
	LOGWARN << "Signal received: "<<signal;
	boost::interprocess::named_condition cond(boost::interprocess::open_or_create,SERVER_CLOSE_COND);
	cond.notify_all();
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
	}
	CATCH
}


//////////////////////////////////////////////////////////////////////////
/*
 * Core function, construct main objects, start daemon if possible, read config files
 * and parses input options if any.
 * @param argc - in, number of comamnd line arguments including name of the program
 * @param *argv[] - in, array with command line arguments
 * @return int - return code of the program execution
 */
int main(int argc, char *argv[])
{
	try
	{
		LOGEMPTY << "Starting process...";
		using namespace IPC;

		// At the very beginning construct two main singletons, one for acquiring config
		// settings, another is for IPC routines (thread pool, listeners, task scheduling)
		boost::shared_ptr<CIPCModule> ipc(&CIPCModule::Instance(),boost::mem_fn(&CIPCModule::Destroy));
		boost::shared_ptr<CConfigurationModule> config(&CConfigurationModule::Instance(),boost::mem_fn(&CConfigurationModule::Destroy));

		// Let's find out if there is another instance of server running. If it is then
		// the only way we can go in this instance is to notify old process about new
		// params or options passed through the command line
		bool bFirstLaunch = ipc->IsFirstInstance();
		if (!config->ProcessServerOptions(bFirstLaunch,argc,argv))
		{
			// assuming that some vital options were incorrect or user was printing
			// config information only
			return 0;
		}

		LOGDEBUG << "Configuration file processed: " << CONFIG_FILE;
		WatchSignals();
		if (bFirstLaunch)
		{
			int iDaemonMode = 0;
			config->GetSetting(eCONFIG_DAEMON_MODE,iDaemonMode);

			// Doesn't matter if the function below fails - just continue
			// working in non-daemon mode then, but log the warning
			if (iDaemonMode == 1)
			{
				LOGDEBUG << "Launching daemon";
				if (daemon(0,0) < 0)
				{
					LOGERROR << "Unable to launch daemon process, err="<<errno;
					//let's try working as a single process?
				}
			}

			ipc->CreateMessageQueue();

			//it's first instance of the service, let's run through configs and start listeners
			std::string strDataRegisterFile;
			int iSendTimeout;
			int iMaintMode;
			int iThreadPoolSize;
			config->GetSetting(eCONFIG_DATA_FILE,strDataRegisterFile);
			config->GetSetting(eCONFIG_SLEEP,iSendTimeout);
			config->GetSetting(eCONFIG_MAINT,iMaintMode);
			config->GetSetting(eCONFIG_THREAD_POOL,iThreadPoolSize);
			ipc->SetupThreadPool(iThreadPoolSize,strDataRegisterFile,iSendTimeout);
			ipc->SetMaintenanceMode(iMaintMode);
			ipc->SetupIPSettings();
			//launch TCP stuff
			boost::thread threadTCPListener( boost::bind(&CIPCModule::StartTCPListener,ipc.get()) );
			boost::thread threadTCPSelector( boost::bind(&CIPCModule::StartTCPSelector,ipc.get()) );
			//launch UDP stuff
			boost::thread threadUDPListener( boost::bind(&CIPCModule::StartUDPListener,ipc.get()) );

			using namespace boost::interprocess;
			boost::shared_ptr<named_mutex> spMutex(new named_mutex(open_or_create, SERVER_CLOSE_MUTEX), boost::bind(&named_mutex::remove,SERVER_CLOSE_MUTEX));
			scoped_lock<named_mutex> lock(*spMutex.get());
			boost::shared_ptr<named_condition> spClosedCond(new named_condition(open_or_create,SERVER_CLOSE_COND), boost::bind(&named_condition::remove,SERVER_CLOSE_COND));
			LOGDEBUG << "Enter wait condition in main thread";
			spClosedCond->wait(lock);
		}
		else
		{
			//this is for the process being launched the second time - it's intended for passing new options
			//to existent process only
			ipc->ApplyServerOptionsRemotely();
		}

		LOGDEBUG << "Exiting process";
	}
	catch (...)
	{
		LOGFATAL << "Unrecoverable issue in the main module, exiting program.";
		return 1;
	}
	LOGDEBUG << "Quit.";
	return 0;
}

