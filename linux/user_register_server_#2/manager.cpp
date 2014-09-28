
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <errno.h>

#include <iostream>

#include "server.h"
#include "logger.h"
#include "config.h"
#include "queue.h"
#include "datafile.h"

#include "manager.h"

using namespace std;

// PID file name for daemon
static const char * const LOCK_FILE_NAME = "/var/run/skypetest.pid";

// Constructor
// configFile - filename of the configuration file
// forceNoPid=true causes MegaManager to start in daemon mode even if it fails to create PID file
// If configuration file states daemon mode = 1 then daemon starts and parent process exits right in constuctor
MegaManager::MegaManager(const char *configFile, bool forceNoPid)
   : _lockFd(-1),
     _config(0),
     _server(0),
     _queue(0),
     _data(0)
{
   // Get current working directory to store. Will be changed if daemon starts
   char workingDir[1024];
   getcwd(workingDir, sizeof(workingDir));

   // Read config
   _config = new TestConfig(workingDir, configFile);
   // Configure logger
   TestLogger *log = TestLogger::instance();
   log->setLevel(_config->logLevel());

   // Log accepted configuration
   string configStr;
   _config->dumpConfig(configStr);
   log->debug("Current configuration:\n%s", configStr.c_str());

   
   // Start daemon if configured so
   if(_config->daemon())
   {
      if(!_createDaemon(forceNoPid))
      {
         log->fatal("Failed to start server in daemon mode");
         ::exit(1);
      }
   }
}

// MegaManager desctructor
// Deletes all entities and removes lock file
MegaManager::~MegaManager()
{
   delete _queue;
   delete _server;
   delete _data;
   delete _config;

   _unlock();
}

// Starts server's and queue's threads
bool MegaManager::run()
{
   if(!_config)
      return false;

   // Create server
   _server = new TestServer(_config->tcpIface(), _config->tcpPort(), _config->udpIface(), _config->udpPort());

   // Create data storage adaptor
   _data = new DataFile(_config->datafile());

   // Create and config requests processing queue
   _queue = new TestQueue(*_data, _config->sleep(), _config->maintenance());

   // Start server with defined processor (queue will be started by server)
   _server->start(_queue);

   // Occupy main thread by config file changes detection
   _trackConfigChanges();
   
   return true;
}

// Creates daemon process
// forceNoPid=true causes MegaManager to start in daemon mode even if it fails to create PID file
bool MegaManager::_createDaemon(bool forceNoPid)
{
   // Change dir to root for best survival
   chdir("/");
   
   // try to create the lock file
   _lockFd = open(LOCK_FILE_NAME, O_RDWR | O_CREAT | O_EXCL, 0644);
   
   if(_lockFd == -1 && !forceNoPid)
   {
      // Failed to create PID file. Let's investigate the reason...
      
      if(errno == EACCES) // Permission denied
      {
         cerr << endl << "Permission denied to create " << LOCK_FILE_NAME << endl;
         cerr << "Start daemon with sudo or use -f to force no PID file creation" << endl << endl;
         return false;
      }

      // Perhaps lockfile already exists
      FILE *lock_file = fopen(LOCK_FILE_NAME, "r");

      if(lock_file == 0)
      {
         // Failed t open file for reading
         perror("Can't open lockfile");
         return false;
      }
      else
      {
         // File exists. Let's read PID.
         char pid_str[8];
         if(fgets(pid_str, 8, lock_file))
         {
            unsigned long lockPID = strtoul(pid_str, 0, 10);
         
            // Check that process alive         
            int killResult = kill(lockPID, 0);
         
            if(killResult==0)
            {
               cerr << endl << "ERROR: A lock file " << LOCK_FILE_NAME << " has been detected. There is an active process with PID " << lockPID << endl;
            }
            else
            {
               if(errno == ESRCH) // non-existent process
               {
                  cerr << endl << "ERROR: A lock file " << LOCK_FILE_NAME << " has been detected. No active process with PID " << lockPID << " found." << endl;
                  cerr << "Try to delete lock file and start daemon again." << endl << endl;
               }
               else
               {
                  perror("Could not lock file");
               }
            }
         }
         else
            perror("Could not read lock file");

         fclose(lock_file);
         return false;
      }
   }

   // PID file created or forced no pid file
   
   if(_lockFd != -1)
   {
      // Set lock on lock file
      struct flock exclusiveLock;
      exclusiveLock.l_type=F_WRLCK;
      exclusiveLock.l_whence=SEEK_SET;
      exclusiveLock.l_len=exclusiveLock.l_start=0;
      exclusiveLock.l_pid=0;
      int lockResult = fcntl(_lockFd, F_SETLK, &exclusiveLock);
   
      if(lockResult < 0) // can't get a lock
      {
         close(_lockFd);
         perror("Can't get lock file");
         return false;
      }
   }

   // Create child daemon process to let parent exit immediately
   switch(fork())
   {
      case 0: // child
         break;

      case -1: // something really bad happend
         perror("Daemon fork failed");
         return false;
         break;

      default: // parent. Exit immediately
         ::exit(0);
         break;
   }

   // Run in a new session
   if(setsid() < 0)
      return false;

   if(_lockFd != -1)
   {
      // Write PID to lock file
      if(ftruncate(_lockFd, 0) < 0)
         return false;

      char pid_str[8];
      snprintf(pid_str, 8, "%d\n", (int)getpid());
      write(_lockFd, pid_str, strlen(pid_str));
   }
   
   // point standard outputs to /dev/null
   close(0);
   close(1);
   close(2);
   int fd = open("/dev/null", O_RDWR); // stdin (0)
   dup(fd); // stdout (1)
   dup(fd); // stderr (2)

   // Daemon started
   return true;
}

// Blocking function that tracks changes of configuration file for dynamic reconfiguration
void MegaManager::_trackConfigChanges()
{
   static const int EVENT_SIZE = sizeof (struct inotify_event);
   static const int BUF_LEN = 1024 * ( EVENT_SIZE + 16 );
   char buffer[BUF_LEN];

   TestLogger *log = TestLogger::instance();

   // Get a config filename
   string filename = _config->filename();
   string dirName(".");

   // Extract dirname from pathname
   size_t slashPos = filename.find_last_of('/');
   if(slashPos != string::npos)
   {
      dirName = filename.substr(0, slashPos);
      filename.erase(0, slashPos + 1);
   }
   log->debug("Tracking changes of %s/%s", dirName.c_str(), filename.c_str());

   // Using inotify
   int fd = inotify_init();

   if ( fd < 0 )
   {
      log->error("Failed to start watching: %s", strerror(errno));
      return;
   }

   // Using inotify to track parent directory changes (file creation, moving and changing)
   int wd = inotify_add_watch( fd, dirName.c_str(), IN_MODIFY | IN_CREATE | IN_MOVED_TO);

   // Do it infinitely in blocking mode
   while(1)
   {
      // Read events on parent dir
      int length = read( fd, buffer, BUF_LEN );

      if ( length < 0 )
         log->error("Failed to get changes event: %s", strerror(errno));

      int i = 0;
      while(i < length)
      {
         struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
         if ( event->len  && filename == event->name)
         {
            log->debug("Config file created or changed");
            // Process configuration changes
            _configChanged();
         }
         i += EVENT_SIZE + event->len;
      }
   }

   ( void ) inotify_rm_watch( fd, wd );
   ( void ) close( fd );
}

// Removes PID file if created
void MegaManager::_unlock()
{
   if(_lockFd != -1)
   {
      close(_lockFd);
      unlink(LOCK_FILE_NAME);
      _lockFd = -1;
   }
}

// Causes server to shutdown immediatelly (on critical signal) and exit application!
void MegaManager::fatalShutdown()
{
   _unlock();
   ::exit(EXIT_FAILURE);
}

// Stops requests processing and exit causes application exiting!
void MegaManager::exit()
{
   if(_server)
      _server->stop();
   _unlock();
   ::exit(EXIT_SUCCESS);
}

// Callback function called on configuration file changes
void MegaManager::_configChanged()
{
   // Read new configuration values
   _config->readConfigFile();

   // Apply values allowed to be changed dynamically
   TestLogger::instance()->setLevel(_config->logLevel());
   _queue->setDelay(_config->sleep());
   _data->setFilename(_config->datafile());
   _queue->setMaintenance(_config->maintenance());
}

