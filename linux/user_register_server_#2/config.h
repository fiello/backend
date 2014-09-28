#pragma once

#include <string>
#include <boost/thread/recursive_mutex.hpp>

// Class TestConfig
//
// Reads and stores configuration from file
// Configuration file format is the following:
// daemon=0
// tcp_if=eth0
// tcp_port=12345
// udp_if=127.0.0.1
// udp_port=54321
// datafile=data.txt
// sleep=1000
// maint=0
// loglevel=0
// 
class TestConfig
{
public:
   // Constructor
   // workingDir is a directory where relative paths are to be looked from
   // configFile - configuration filename
   // Reads configuration and stores in local variables
   TestConfig(const char *workingDir, const char *configFile);

   // Reads configuration file using filename specified in constructor
   void readConfigFile();
   // outputs current configuration to external string in the format of config file
   void dumpConfig(std::string &output) const;

   // Returns full pathname of configuration file
   const std::string& filename() const;

   // Below are the configuration values getters
   int logLevel() const;
   int tcpPort() const;
   int udpPort() const;
   int sleep() const;
   const std::string& tcpIface() const;
   const std::string& udpIface() const;
   const std::string& datafile() const;
   bool daemon() const;
   bool maintenance() const;


protected:
   mutable boost::recursive_mutex _mutex;

   // Checks the values provided as text and  sets local variables it configuration is valid
   // Returns true if parameters name and values are valid and successfully set
   bool _setValue(const char *name, const char *value);

   // Not a configuration value but directory to look relative path (e.g. datafile) from
   std::string _startDir;
   // Configuration file full pathname
   std::string _filename;

   // Configuration values
   int _logLevel;
   int _tcpPort;
   std::string _tcpIface;
   int _udpPort;
   std::string _udpIface;
   int _sleep;
   std::string _datafile;
   bool _daemon;
   bool _maint;
};

