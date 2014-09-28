#pragma once

#include <fstream>
#include <stdarg.h>

// Class TestLogger
// This is a singleton
// Use TestLogger::instance() method to obtain logger pointer
// Logs messages of different level to log file created by filename specified
// Only messages with the level higher than configured will be logged
class TestLogger
{
private:
   // Constructor
   // Gets filename of the log file and opens output stream buffer
   // In case of error on file openning log will be redirected to standard output
   TestLogger(const char *filename);
   // Destructor
   // Closes log file. Never called however )
   ~TestLogger();

   // Static (single) instance of the logger
   static TestLogger *_instance;
   
public:
   // The only accessor of the logger object
   // Creates logger on first call
   // Return pointer to single logger instance
   static TestLogger *instance();

   // Defines log levels
   enum LOG_LEVELS 
   {
      DEBUG_LEVEL = 0,
      WARN_LEVEL  = 1,
      ERROR_LEVEL = 2,
      FATAL_LEVEL = 3,
      LEVELS_SIZE   = 4
   };

   // Main output functions receving text in format of printf command
   void debug(const char *text, ...);
   void warn (const char *text, ...);
   void error(const char *text, ...);
   void fatal(const char *text, ...);

   // Change current level to be logged. Messages with lower level will be ignored
   void setLevel(int newLevel);
   // Current level getter
   int level() const;

protected:
   // Current level
   int _level;
   // output file stream
   std::ofstream _outFile;
   // Universal log function for any level
   void _log(int logLevel, const char *text, va_list args);
};

// Auxillary quick logger accessors for single output
#define LOG_DEBUG   TestLogger::instance()->debug
#define LOG_WARNING TestLogger::instance()->warn
#define LOG_ERROR   TestLogger::instance()->error
#define LOG_FATAL   TestLogger::instance()->fatal
