#pragma once

#include <string>
#include <boost/thread/recursive_mutex.hpp>

// Class DataFile
// Provides safe access to data
// Data is stored in file with specified filename
// Data is not chached and each request causes file access
// Filename can be changed during runtime
class DataFile
{
public:
   // Constructor
   // filename is an initial filename to read/store data
   DataFile(const std::string &filename);

   // Changes filename for further requests
   void setFilename(const std::string &filename);

   // Possible error codes returned by request functions
   typedef enum {
      E_OK = 0,         // Request executed without errors
      E_ERROR = 1,      // Invalid request or some error occured during request execution
      E_NOT_FOUND = 2,  // Username not found
      E_OVERLOADED = 3, // Data file is overloaded. Too much records in the file
      E_INVALID = 4,    // Invalid username or email provided
      E_CONFLICT = 5,   // Username provided already exists in data file
      E_UNAVAILABLE = 6 // Data file can't be accessed for info reading or writing
   } ErrorCode;

   // Register new user request function
   // Validates username, email and puts data to file if possible
   // Returns E_OK if done or appropriate code of error (see above)
   int registerUser(const std::string &username, const std::string &email);

   // Find user's email request function
   // Reads data file and serchs for username. retEmail is used to store email of found user
   // Returns E_OK if done or appropriate code of error (see above)
   int getEmail(const std::string &username, std::string &retEmail) const;
   
protected:
   mutable boost::recursive_mutex _mutex;

   // data file name
   std::string _filename;
};
