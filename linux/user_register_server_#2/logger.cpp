
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdarg.h>

#include "logger.h"

typedef boost::lock_guard<boost::mutex> LOCK;

// Log filename is defined here
#ifndef LOG_FILENAME
#define LOG_FILENAME "skypetest.log"
#endif

using namespace std;

TestLogger* TestLogger::_instance = 0;

// Can define mutexes here because logger is a singleton
static boost::mutex LEVEL_MUTEX;
static boost::mutex FILE_MUTEX;

// Text values for debug lines
static const char * PREFIXES[TestLogger::LEVELS_SIZE] = {
   "DEBUG: ",
   "WARN: ",
   "ERROR: ",
   "FATAL: "
};

// Constructor
// Gets filename of the log file and opens output stream buffer
// In case of error on file openning log will be redirected to standard output
TestLogger::TestLogger(const char *filename)
   : _level(WARN_LEVEL),
     _outFile(filename)
{
   if(!_outFile)
      cerr << "Failed to open file " << filename << " for writing. Dumping to console.\n";
   debug("Logger started");
}

// Destructor
TestLogger::~TestLogger()
{
   _outFile.close();
}

// The only accessor of the logger object
// Creates logger on first call
// Return pointer to single logger instance
TestLogger* TestLogger::instance()
{
   static boost::mutex test_logger_mutex;
   LOCK lock(test_logger_mutex);
   if(!_instance)
      _instance = new TestLogger(LOG_FILENAME);
   return _instance;
}

// Change current level to be logged. Messages with lower level will be ignored
void TestLogger::setLevel(int newLevel)
{
   LOCK lock(LEVEL_MUTEX);
   _level = newLevel;
}

// Current level getter
int TestLogger::level() const
{
   LOCK lock(LEVEL_MUTEX);
   return _level;
}

// Universal log function for any level
void TestLogger::_log(int logLevel, const char * text, va_list args)
{
   if(logLevel < level())
      return;

   // Avoid parallel access to the file
   LOCK lock(FILE_MUTEX);

   // Filling the buffer with format and values
   static char buffer[1024];
   vsnprintf (buffer, sizeof(buffer), text, args);

   if(logLevel >= LEVELS_SIZE - 1)
      logLevel = LEVELS_SIZE - 1;

   const char *prefix = PREFIXES[logLevel];
   if(text == 0)
      text = "";

   // Add time for each record
   boost::posix_time::ptime currentTime = boost::posix_time::microsec_clock::local_time();
 
   ostream& output = _outFile ? _outFile : cout;
   output << currentTime << ": " << prefix << buffer << endl;
}

// Macro that creates log level functions for different level different only with name and level
#define CREATE_LOG_FUNCTION(name, logLevel)     \
   void TestLogger::name(const char *text, ...) \
   {                                            \
      if(logLevel < level())                    \
         return;                                \
      va_list args;                             \
      va_start (args, text);                    \
      _log(logLevel, text, args);               \
      va_end (args);                            \
   }

// Create functions definition
CREATE_LOG_FUNCTION(debug, DEBUG_LEVEL)
CREATE_LOG_FUNCTION(warn , WARN_LEVEL )
CREATE_LOG_FUNCTION(error, ERROR_LEVEL)
CREATE_LOG_FUNCTION(fatal, FATAL_LEVEL)
      
