#pragma once

class TestConfig;
class TestServer;
class TestQueue;
class DataFile;

// Class MegaManager
// Creates and manages all major entities of the application:
//    TCP/UDP server, configuration, queue, data file adaptor
// Can be started as daemon (according to configuration value)
// Tracks configuration file changes (dynamic config)
class MegaManager
{
public:
   // Constructor
   // configFile - filename of the configuration file
   // forceNoPid=true causes MegaManager to start in daemon mode even if it fails to create PID file
   // If configuration file states daemon mode = 1 then daemon starts and parent process exits right in constuctor
   MegaManager(const char *configFile, bool forceNoPid = false);

   // MegaManager desctructor
   // Deletes all entities and removes lock file
   ~MegaManager();

   // Starts server's and queue's threads
   bool run();
   // Stops requests processing and exit causes application exiting!
   void exit();
   // Causes server to shutdown immediatelly (on critical signal) and exit application!
   void fatalShutdown();
      
private:
   // Removes PID file if created
   void _unlock();
   // Creates daemon process
   // forceNoPid=true causes MegaManager to start in daemon mode even if it fails to create PID file
   bool _createDaemon(bool forceNoPid);
   // Blocking function that tracks changes of configuration file for dynamic reconfiguration
   void _trackConfigChanges();
   // Callback function called on configuration file changes
   void _configChanged();

   // File descriptor of PID file
   int _lockFd;

   // Internal entities
   TestConfig *_config;
   TestServer *_server;
   TestQueue  *_queue;
   DataFile   *_data;
};

