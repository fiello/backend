
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <map>
#include <boost/thread/locks.hpp>
#include <boost/regex.hpp>

#include "logger.h"
#include "config.h"

using namespace std;

typedef boost::lock_guard<boost::recursive_mutex> RLOCK;

static const char DAEMON_STR[]   = "daemon";
static const char TCP_IF_STR[]   = "tcp_if";
static const char TCP_PORT_STR[] = "tcp_port";
static const char UDP_IF_STR[]   = "udp_if";
static const char UDP_PORT_STR[] = "udp_port";
static const char DATAFILE_STR[] = "datafile";
static const char SLEEP_STR[]    = "sleep";
static const char MAINT_STR[]    = "maint";
static const char LOGLEVEL_STR[] = "loglevel";

// Constructor
// workingDir is a directory where relative paths are to be looked from
// configFile - configuration filename
// Reads configuration and stores in local variables
TestConfig::TestConfig(const char *startDir, const char *configFile)
   : _startDir(startDir),
     _filename(configFile),
     _logLevel(0), 
     _tcpPort(12345),
     _tcpIface("eth0"),
     _udpPort(54321),
     _udpIface("lo"),
     _sleep(1000),
     _datafile("data.txt"),
     _daemon(false)
{
   // Set the full pathname for _filename
   if(_startDir[_startDir.size()] != '/') // No trailing slash
      _startDir += '/';
   
   if(_filename[0] != '/') // path is relative
      _filename.insert(0, _startDir);

   // Read values immediately
   readConfigFile();
}

// Reads configuration file using filename specified in constructor
void TestConfig::readConfigFile()
{
   TestLogger *log = TestLogger::instance();
   RLOCK lock(_mutex);
   log->debug("Openning file");
   ifstream ifile(_filename.c_str());
   if(!ifile)
   {
      log->error("Can't open configuration file '%s'", _filename.c_str());
      return;
   }
   else
   {
      std::string line;
      if(!ifile.good())
         log->error("Can't read config file '%s'", _filename.c_str());

      log->debug("Reading file");
      boost::smatch matches;
      static const boost::regex CONFIG_LINE_EXP("[[:blank:]]*([^#].*?)[[:blank:]]*=[[:blank:]]*(.*?)[[:blank:]]*");
      while(ifile.good())
      {
         getline(ifile, line);
         if(!ifile)
            break;
         if(boost::regex_match( line, matches, CONFIG_LINE_EXP ))
         {
            std::string name  = matches.str(1);
            std::string value = matches.str(2);
            log->debug("Found config parameter: '%s'='%s'", name.c_str(), value.c_str());
            _setValue(name.c_str(), value.c_str());
         }
         else
         {
            log->debug("not a config line (%s), ignoring", line.c_str());
         }
      }
      ifile.close();
   }
}

// Checks the values provided as text and  sets local variables it configuration is valid
// Returns true if parameters name and values are valid and successfully set
bool TestConfig::_setValue(const char *name, const char *value)
{
   TestLogger *log = TestLogger::instance();

   // Check string values first
   if(strncasecmp(name, TCP_IF_STR, sizeof(TCP_IF_STR)) == 0)
   {
      RLOCK lock(_mutex);
      _tcpIface = value;
   }
   else if(strncasecmp(name, UDP_IF_STR, sizeof(UDP_IF_STR)) == 0)
   {
      RLOCK lock(_mutex);
      _udpIface = value;
   }
   else if(strncasecmp(name, DATAFILE_STR, sizeof(DATAFILE_STR)) == 0)
   {
      RLOCK lock(_mutex);
      _datafile = value;
      if(_datafile[0] != '/') // Path is relative
         _datafile.insert(0, _startDir);
   }
   else
   {
      // Other values are to be numberic
      char *endptr = 0;
      int number = strtol(value, &endptr, 10);
      if(!endptr || *endptr != '\0')
      {
         log->error("Invalid value '%s' for parameter '%s'.", value, name);
         return false;
      }

      // process values that may be negative here

      // ...

      // Other values are positive
      if(number < 0)
      {
         log->error("Negative number for '%s' for parameter '%s' is invalid.", value, name);
         return false;
      }
      
      if(strncasecmp(name, DAEMON_STR, sizeof(DAEMON_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _daemon = number;
      }
      else if(strncasecmp(name, TCP_PORT_STR, sizeof(TCP_PORT_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _tcpPort = number;
      }
      else if(strncasecmp(name, UDP_PORT_STR, sizeof(UDP_PORT_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _udpPort = number;
      }
      else if(strncasecmp(name, LOGLEVEL_STR, sizeof(LOGLEVEL_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _logLevel = number;
      }
      else if(strncasecmp(name, MAINT_STR, sizeof(MAINT_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _maint = number;
      }
      else if(strncasecmp(name, SLEEP_STR, sizeof(SLEEP_STR)) == 0)
      {
         RLOCK lock(_mutex);
         _sleep = number;
      }
      else
      {
         LOG_ERROR("Invalid config parameter: '%s'", name);
         return false;
      }
   }
   return true;
}

// outputs current configuration to external string in the format of config file
void TestConfig::dumpConfig(std::string &output) const
{
   ostringstream strstr;
   {
      RLOCK lock(_mutex);
      strstr << DAEMON_STR   << "=" << (int)daemon() << endl;
      strstr << TCP_IF_STR   << "=" << tcpIface()    << endl;
      strstr << TCP_PORT_STR << "=" << tcpPort()     << endl;
      strstr << UDP_IF_STR   << "=" << udpIface()    << endl;
      strstr << UDP_PORT_STR << "=" << udpPort()     << endl;
      strstr << DATAFILE_STR << "=" << datafile()    << endl;
      strstr << SLEEP_STR    << "=" << sleep()       << endl;
      strstr << MAINT_STR    << "=" << (int)maintenance() << endl;
      strstr << LOGLEVEL_STR << "=" << logLevel()    << endl;
   }
   output = strstr.str();
}

// Returns full pathname of configuration file
const std::string& TestConfig::filename() const
{
   RLOCK lock(_mutex);
   return _filename;
}

int TestConfig::logLevel() const
{
   RLOCK lock(_mutex);
   return _logLevel;
}

int TestConfig::tcpPort() const
{
   RLOCK lock(_mutex);
   return _tcpPort;
}

const std::string& TestConfig::tcpIface() const
{
   RLOCK lock(_mutex);
   return _tcpIface;
}

int TestConfig::udpPort() const
{
   RLOCK lock(_mutex);
   return _udpPort;
}

const std::string& TestConfig::udpIface() const
{
   RLOCK lock(_mutex);
   return _udpIface;
}

int TestConfig::sleep() const
{
   RLOCK lock(_mutex);
   return _sleep;
}

const std::string& TestConfig::datafile() const
{
   RLOCK lock(_mutex);
   return _datafile;
}

bool TestConfig::daemon() const
{
   RLOCK lock(_mutex);
   return _daemon;
}

bool TestConfig::maintenance() const
{
   RLOCK lock(_mutex);
   return _maint;
}
