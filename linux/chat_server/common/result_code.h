/**
 *  \file
 *  \brief     Holds common results codes
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef CS_RESULT_CODE_H
#define CS_RESULT_CODE_H

/// typedef to be used as a return value by class members
typedef unsigned int result_t;

namespace cs
{
namespace result_code
{

/// List of result codes recommended for usage across the application.
/// The one starts with 's' means success, the one starts with 'e' means
/// incorrect result code
enum
{
    sOk,

    // common errors
    eFail,
    eNotFound,
    eNotReady,
    eInvalidArgument,
    eBufferOverflow,
    eConnectionClosed,
    eAlreadyDefined,
    eUnexpected
};

} // namespace result_code
} // namespace cs

/**
 *  \namespace    cs::result_code
 *  \brief        Holds common results codes
 */

#endif // CS_RESULT_CODE_H
