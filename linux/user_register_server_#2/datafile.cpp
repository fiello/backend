
#include <fstream>
#include <boost/thread/locks.hpp>
#include <boost/regex.hpp>

#include "logger.h"
#include "datafile.h"

typedef boost::lock_guard<boost::recursive_mutex> RLOCK;

// Maximum number of records allowed to be stored in the file
static const int MAX_RECORDS_ALLOWED = 100;

// Gets line by line from input stream and searchs for user record.
// If record is found stores found email in emailOut and doesn't read more lines
// Returns tru if username found or false otherwise
static bool findEmail(std::istream &is, const std::string &user, std::string &emailOut);

// Gets line by line from input stream and searchs for user record.
// linesRead parameter is actual if record is not found to calculate lines number in the file
// Returns true if record is found and false otherwise
static bool checkUser(std::istream &is, const std::string &user, int &linesRead);

// Constructor
// filename is an initial filename to read/store data
DataFile::DataFile(const std::string &filename)
   : _filename(filename)
{
}

// Changes filename for further requests
void DataFile::setFilename(const std::string &filename)
{
   RLOCK lock(_mutex);
   _filename = filename;
}

// Auxilliary function that trims spaces and tab characters at the beining and at the end of string
// It is redundant with Request class that does it by regex but let it be just in case
static void trimSpaces(std::string &str)
{
   static const char * spaces = " \t";
   size_t first = str.find_first_not_of(spaces);
   if(first == std::string::npos) // only spaces
   {
      str = "";
      return;
   }
   str.erase(0, first);
   
   size_t last = str.find_last_not_of(spaces);
   if(last != std::string::npos && last < str.size())
      str.erase(last+1);
}

// Trims spaces (changes parameter) and check that username is valid
static bool validateUsername(std::string &username)
{
   trimSpaces(username);
   static const boost::regex exp("[^;\\r\\n]+");
   return boost::regex_match( username, exp );
}

// Trims spaces (changes parameter) and check that email is valid
static bool validateEmail(std::string &email)
{
   trimSpaces(email);
   static const boost::regex exp("[a-zA-Z0-9_\\.]+@([a-zA-Z0-9]+\\.)+[a-zA-Z]{2,4}");
   return boost::regex_match( email, exp );
}

// Register new user request function
// Validates username, email and puts data to file if possible
// Returns E_OK if done or appropriate code of error (see above)
int DataFile::registerUser(const std::string &user, const std::string &mail)
{
   int result = E_OK;

   std::string username(user);
   std::string email(mail);

   TestLogger *log = TestLogger::instance();

   if(!validateUsername(username))
   {
      log->warn("User name '%s' is not valid", user.c_str());
      return E_INVALID;
   }
   if(!validateEmail(email))
   {
      log->warn("E-mail '%s' is not valid.", mail.c_str());
      return E_INVALID;
   }

   RLOCK lock(_mutex); // lock access to file for whole operation

   // Try to open file for boh reading and writing
   // Thus even if record already exist but file is write protected the error code will be E_UNAVAILABLE
   std::fstream file(_filename.c_str());
   if(!file)
      return E_UNAVAILABLE;
   
   // Check that name doesn't exist
   int linesRead = 0;
   if(!checkUser(file, username, linesRead))
   {
      if(linesRead > MAX_RECORDS_ALLOWED)
      {
         log->error("Too many records in file");
         result = E_OVERLOADED;
      }
      else
      {
         // Add new record
         log->debug("Adding new record to file (lines read = %d)", linesRead);
         file.clear();
         file << username << ';' << email << "\n";
         if(!file)
         {
            log->error("Failed to add record to file");
            result = E_UNAVAILABLE;
         }
      }
   }
   else
   {
      // Username already exists in file
      result = E_CONFLICT;
   }
   file.close();
   
   return result;
}

// Find user's email request function
// Reads data file and serchs for username. retEmail is used to store email of found user
// Returns E_OK if done or appropriate code of error (see above)
int DataFile::getEmail(const std::string &username, std::string &retEmail) const
{
   RLOCK lock(_mutex);

   int result = E_OK;
   
   // Open file as input stream
   std::ifstream ifile(_filename.c_str());
   if(!ifile)
      return E_UNAVAILABLE;

   // Check that name doesn't exist
   if(!findEmail(ifile, username, retEmail))
      result = E_NOT_FOUND;

   ifile.close();
   
   return result;
}

// Gets line by line from input stream and searchs for user record.
// If record is found stores found email in emailOut (if not null) and doesn't read more lines
// Stores number of lines read from file in linesRead (if not null).
// Returns true if username found or false otherwise
bool findUser(std::istream &is, const std::string &user, std::string *emailOut = 0, int *linesRead = 0)
{
   std::string line;
   TestLogger *log = TestLogger::instance();
   log->debug("Looking for user '%s'", user.c_str());

   while(!is.eof()) // should we look for more than MAX_RECORDS_ALLOWED (if file edited externally)?
   {
      getline(is, line);

      if(linesRead)
         ++(*linesRead);
      
      // Parse line: e.g. 'John Smith;john.smith@somewhere.com'
      size_t delimPos = line.find(';');
      if(delimPos == std::string::npos)
         continue;

      std::string username = line.substr(0, delimPos);
      if(username.size() == user.size() && strncasecmp(username.c_str(), user.c_str(), username.size()) == 0)
      {
         if(emailOut)
         {
            *emailOut = line.substr(delimPos+1);
            log->debug("Found e-mail: '%s'", emailOut->c_str());
         }
         return true;
      }
   }
   log->debug("Username: '%s' not found", user.c_str());
   return false;
}

// Gets line by line from input stream and searchs for user record.
// If record is found stores found email in emailOut and doesn't read more lines
// Returns true if username found or false otherwise
static bool findEmail(std::istream &is, const std::string &user, std::string &emailOut)
{
   return findUser(is, user, &emailOut, 0);
}

// Gets line by line from input stream and searchs for user record.
// linesRead parameter is actual if record is not found to calculate lines number in the file
// Returns truee if record is found and false otherwise
static bool checkUser(std::istream &is, const std::string &user, int &linesRead)
{
   return findUser(is, user, 0, &linesRead);
}
