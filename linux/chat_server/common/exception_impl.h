/**
 *  \file
 *  \brief     Holds BasicException class implementation
 *  \details   Basic exception wrapper is capable of storing some extra information
 *             on top of std::exception class : user-defined result code, filename, function
 *             and line number where exception was re-thrown from.
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_HELPERS_EXCEPTION_IMPL_H
#define CS_HELPERS_EXCEPTION_IMPL_H

#include "result_code.h"
// third-party
#include <sstream>
#include <exception>

namespace cs
{
namespace helpers
{
namespace exception_impl
{

/**
 *  \class     cs::helpers::exception_impl::BasicException
 *  \brief     Base class to make exception handling easier
 *  \details   Class provides additional functionality for easy exception handling,
 *             that is: store error result code, extended error message, support
 *             streaming operator '<<' to compose error message in place
 */
class BasicException : public std::exception
{
public:
   /**
    * Constructor.
    * @param resultCode - result code the exception is throw with
    * @param fileName - name of the source file the exception is thrown in
    * @param lineNumber - line number in the source file the exception is thrown at
    * @param functionName - function name the exception is thrown from
    */
   BasicException(
         const result_t resultCode,
         const char* fileName,
         int lineNumber,
         const char* functionName
      )
      : m_resultCode(resultCode)
   {
      std::ostringstream output;
      output << "BasicException: \nfile: " << fileName
         << "\nfunction: " << functionName
         << "\nline: " << lineNumber
         << "\nerror message: ";
      m_errorMessage = output.str();
   }

   /**
    * Destructor. Has to specify one since automatically defined destructor
    * will have no throw specifier. This means it will have another definition
    * and will cause a compilation error since base class has this specifier
    */
   virtual ~BasicException() throw() {};

   /**
    * Override of base class method. Intended for acquiring error message data.
    * @returns - pointer to the constant error message data
    */
   virtual const char* what() const throw()
   {
      return m_errorMessage.c_str();
   }

   /**
    * Override of streaming operator. Can be used to compose error message
    * in order to provide more details about error occurred
    * @param obj - reference to the template parameter to be added to error message
    * @returns - reference to the BasicException object
    */
   template <typename T>
   BasicException& operator<< (const T& obj)
   {
      std::ostringstream stream;
      stream << obj;
      m_errorMessage += stream.str();
      return *this;
   }

   /**
    * Specific operator for easy grabbing of result code
    * @returns - result code the exception was throw with
    */
   operator result_t() const
   {
      return m_resultCode;
   }

private:
   /// result code that the exception was thrown with
   result_t    m_resultCode;
   /// error message
   std::string m_errorMessage;
};

} // namespace exception_impl
} // namespace helpers
} // namespace cs

/**
 *  \namespace  cs::helpers::exception_impl
 *  \brief      Holds implementation of BasicException class
 */

#endif // CS_HELPERS_EXCEPTION_IMPL_H
