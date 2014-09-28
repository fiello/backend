
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "manager.h"

using namespace std;

static const char * const DEFAULT_CONFIG_FILE = "skypetest.conf";

void printUsage(const char *selfname)
{
   cerr << "Usage: " << selfname << " [-f] [<config_file>]" << endl;
   cerr << "  -h            : print this help" << endl;
   cerr << "  -f            : ignore PID file creation problems (for daemon mode only)" << endl;
   cerr << "  <config_file> : configuration file" << endl;
   cerr << endl;
}

// Global pointer to main manager object
static MegaManager *megaManager = 0;

// Installs signal handlers for the application.
// Mainly used for daemon mode to unlink PID file
void installSignalHandlers();

// Fatal signals handler
void exitImmediately(int signum);

// Non fatal but user signal handler
void stopExection(int signum);

// This is MAIN
int main(int argc, char *argv[])
{
   // Read command line args
   char opt;
   bool forceNoPid = false;
   while ((opt = getopt(argc, argv, "fh")) != -1)
   {
      switch (opt)
      {
         case 'f':
            forceNoPid = true;
            break;
         case 'h':
            printUsage(argv[0]);
            return 1;
         default: /* '?' */
            cerr << "Wrong input arguments." << endl;
            printUsage(argv[0]);
            return 1;
      }
   }

   const char *configFile = DEFAULT_CONFIG_FILE;

   // Get config file name from command line option
   if (optind < argc)
      configFile = argv[optind];

   // Create manager
   megaManager = new MegaManager(configFile, forceNoPid);

   installSignalHandlers();

   // Start servers
   bool result = megaManager->run();
   
   delete megaManager;
   
   return result ? 0 : 1;
}

// Installs signal handlers for the application.
// Mainly used for daemon mode to unlink PID file
void installSignalHandlers()
{
   // Ignore not important
   signal(SIGUSR2, SIG_IGN);	
   signal(SIGPIPE, SIG_IGN);
   signal(SIGALRM, SIG_IGN);
   signal(SIGURG , SIG_IGN);
   signal(SIGXCPU, SIG_IGN);
   signal(SIGXFSZ, SIG_IGN);
   signal(SIGVTALRM, SIG_IGN);
   signal(SIGPROF, SIG_IGN);
   signal(SIGIO  , SIG_IGN);
   signal(SIGCHLD, SIG_IGN);

   static struct sigaction sa;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags=0;

   // Fatal signals 
   sa.sa_handler=exitImmediately;

   sigaction(SIGQUIT  , &sa, NULL);
   sigaction(SIGILL   , &sa, NULL);
   sigaction(SIGTRAP  , &sa, NULL);
   sigaction(SIGABRT  , &sa, NULL);
   sigaction(SIGPWR   , &sa, NULL);
   sigaction(SIGSYS   , &sa, NULL);
   sigaction(SIGHUP   , &sa, NULL);
   sigaction(SIGTERM  , &sa, NULL);
   // Uncomment for PID deletion. Commented to catch core files for debugging
   //   sigaction(SIGBUS   , &sa, NULL);
   //   sigaction(SIGSEGV  , &sa, NULL);
		
   sa.sa_handler=stopExection;
   sigaction(SIGUSR1  , &sa, NULL);
}

// Fatal signals handler
void exitImmediately(int signum)
{
   cerr << "Got termination signal. Shutting down." << endl;
   if(megaManager)
      megaManager->fatalShutdown();
   exit(EXIT_FAILURE);
}

// Non fatal but user signal handler
void stopExection(int signum)
{
   cout << "Got stopping signal. Exiting..." << endl;
   if(megaManager)
      megaManager->exit();
   else
      exit(EXIT_SUCCESS);
}
