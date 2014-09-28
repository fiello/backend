/**
 *  \file
 *  \brief     Holds ExceptionDispatcher class implementation
 *  \details   Holds implementation of the helper class to be used for exception
 *             dispatching across the application. Depends on Logger library and 
 *             requires it to be linked with target module where this 
 *             header is included
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_HELPERS_EXCEPTION_DISPATCHER_H
#define CS_HELPERS_EXCEPTION_DISPATCHER_H

#include "exception_impl.h"
#include <logger/logger.h>
// third-party
#include <string.h>

namespace cs
{
namespace helpers
{

/**
 *  \class cs::helpers::ExceptionDispatcher
 *  \brief Helper class to be used for dispatching exceptions and tracing
 *         result codes / error messages.
 */
class ExceptionDispatcher
{
public:
   /**
   * Re-throws existing exception and captures it in an appropriate catch block in
   * order to trace error message and error description. Should be used in 'catch'
   * section of another function. Never throws.
   * @param description - reference to the string with error description. Common usage is
   *                      to pass current function name where exception was raised.
   * @returns - result code the exception was thrown with. In case of unknown exception
   *            eUnexpected is returned.
   */
   static result_t Dispatch(const std::string& description)
   {
      try
      {
         throw;
      }
      catch(const exception_impl::BasicException &ex)
      {
         LOGERR << ex.what();
         result_t code = (result_t)ex;
         return code;
      }
      catch(const std::exception &ex)
      {
         LOGERR << description << " : Unexpected exception : " << ex.what();
         return result_code::eUnexpected;
      }
   }
};

} // namespace helpers
} // namespace cs


/// basic macros to throw exception with the specific result code
#define THROW_BASIC_EXCEPTION(resultCode)\
   throw cs::helpers::exception_impl::BasicException(\
      resultCode, __FILE__, __LINE__, BOOST_CURRENT_FUNCTION)

/// helper macros to throw exception with InvalidArgument result code
/// (most common case in cross-component calls where input arguments must
/// be validated according to component contract)
#define THROW_INVALID_ARGUMENT\
   THROW_BASIC_EXCEPTION(cs::result_code::eInvalidArgument)

/// helper macros which throws exception with eFail and provide additional
/// information for errorCode assuming it's a Linux OS network error code
#define THROW_NETWORK_EXCEPTION(errorCode)\
   THROW_BASIC_EXCEPTION(cs::result_code::eFail)\
      << "System error message: " << strerror(errorCode) << ". "

/// helper macros to check boolean expression and throw
/// InvalidArgument exception with extended message description
#define CHECK_ARGUMENT(booleanExpression, msg)\
   if (!(booleanExpression))\
      THROW_INVALID_ARGUMENT << msg;

#endif // CS_HELPERS_EXCEPTION_DISPATCHER_H
