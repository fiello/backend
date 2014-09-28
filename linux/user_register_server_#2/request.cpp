
#include <strings.h>
#include <string>
#include <iostream>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/regex.hpp>

#include "connection.h"
#include "logger.h"
#include "datafile.h"
#include "request.h"

typedef boost::lock_guard<boost::mutex> LOCK;

const char REGISTER_CMD[] = "REGISTER";
const char GET_CMD[]      = "GET";

#define RESP_OK             "200 OK"
#define RESP_CONFLICT       "409 Conflict"
#define RESP_NOT_FOUND      "404 Not Found"
#define RESP_NOT_ACCEPTABLE "406 Not Acceptable"
#define RESP_OVERLOADED     "405 Overloaded"
#define RESP_BAD_REQUEST    "400 Bad request"
#define RESP_UNAVAILABLE    "503 Service unavailable"

// Fills text response according to integer code
static void errorToResponse(int errCode, std::string &response);

// Constructor
// Stores pased request and client connection
// Remembers time of creation if corresponding policy selected
Request::Request(const std::string &request, Connection *connection)
   : _request(request),
     _connection(connection)
#if DELAY_POLICY == DELAY_NO_EARLIER
   , _creationTime(boost::posix_time::microsec_clock::local_time())
#endif
{
}

// Removes closed client's connection
void Request::invalidateConnection()
{
   LOCK lock(_mutex);
   LOG_DEBUG("Connection invalidated");
   _connection = 0;
}

// Connection getter
const Connection* Request::connection() const
{
   LOCK lock(_mutex);
   return _connection;
}

// Request getter
const std::string& Request::request() const
{
   LOCK lock(_mutex);
   return _request;
}

// Main processing function
// Parse request, pass to data and send response to client
// optional delayMsec parameter is used for response sending delay
void Request::execute(DataFile &data, int delay)
{
   TestLogger *log = TestLogger::instance();

   // Calculate time to send response not earlier than
   // Depends on selected policy
#if DELAY_POLICY == DELAY_EXECUTION
   boost::posix_time::ptime responseTime = boost::posix_time::microsec_clock::local_time() + boost::posix_time::millisec(delay);
#elif DELAY_POLICY == DELAY_NO_EARLIER
   boost::posix_time::ptime responseTime = _creationTime + boost::posix_time::millisec(delay);
#endif
   
   std::string req = request();
   log->debug("Processing request '%s'", req.c_str());
   
   // Parse and process request here
   std::string response;

   // REGISTER command matching rule (ags are to be validated by datafile)
   static const boost::regex REGISTER_EXP(
      "[[:blank:]]*REGISTER"
      "[[:blank:]]+username[[:blank:]]*=[[:blank:]]*(.*?)[[:blank:]]*;"
      "[[:blank:]]*email[[:blank:]]*=[[:blank:]]*(.*?)", boost::regex::normal | boost::regex::icase);

   // GET command matching rule (ags are to be validated by datafile)
   static const boost::regex GET_EXP(
      "[[:blank:]]*GET"
      "[[:blank:]]+username[[:blank:]]*=[[:blank:]]*(.*)", boost::regex::normal | boost::regex::icase);

   boost::smatch matches;
   if(boost::regex_match( req, matches, REGISTER_EXP ))
   {
      // This is REGISTER command. Get agruments
      std::string username = matches.str(1);
      std::string email = matches.str(2);
      log->debug("Got REGISTER cmd with username = '%s' and email = '%s'", username.c_str(), email.c_str());

      // Process command and form response
      errorToResponse(data.registerUser(username, email), response);
   }
   else if(boost::regex_match( req, matches, GET_EXP ))
   {
      // This is GET command. Get username
      std::string username = matches.str(1);
      log->debug("Got GET cmd with username = '%s'", username.c_str());

      std::string email;
      // Process command and form response
      errorToResponse(data.getEmail(username, email), response);
      // add found email address to reponse
      if(email.size() > 0)
      {
         response += " email=";
         response += email;
      }
   }
   else
   {
      log->warn("Bad request, command not recognized.");
      response = RESP_BAD_REQUEST;
   }
   
   {
      LOCK lock(_mutex);
      if(!_connection)
      {
         // Connection was closed before this. So we will not wait for response sending.
         log->debug("No open connection for response on '%s'", req.c_str());
         return;
      }
   }

   // Delay execution according to selected policy (see header file for descirption)
#if DELAY_POLICY == DELAY_ADD_SLEEP
   boost::this_thread::sleep(boost::posix_time::millisec(delay));
#else
   boost::this_thread::sleep(responseTime - boost::posix_time::microsec_clock::local_time());
#endif
   
   log->debug("Response for request '%s' is ready: %s", req.c_str(), response.c_str());
   response += "\r\n";

   LOCK lock(_mutex);
   // Check if connection is still alive after delay
   if(_connection)
   {
      _connection->send(response.c_str());
   }
   else
   {
      log->debug("Connection is gone already");
   }
}

// Fills text response according to integer code
static void errorToResponse(int errCode, std::string &response)
{
   switch(errCode)
   {
      case DataFile::E_OK:
         response = RESP_OK;
         break;
      case DataFile::E_ERROR:
         response = RESP_BAD_REQUEST;
         break;
      case DataFile::E_NOT_FOUND:
         response = RESP_NOT_FOUND;
         break;
      case DataFile::E_OVERLOADED:
         response = RESP_OVERLOADED;
         break;
      case DataFile::E_INVALID:
         response = RESP_NOT_ACCEPTABLE;
         break;
      case DataFile::E_CONFLICT:
         response = RESP_CONFLICT;
         break;
      case DataFile::E_UNAVAILABLE:
      default:
         response = RESP_UNAVAILABLE;
         break;
   }
}
