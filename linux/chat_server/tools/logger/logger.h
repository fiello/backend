/**
 *  \file
 *  \brief     General include file to use logger functionality
 *  \details   Defines bunch of helper macroses to be used for easy logging
 *  \author    Dmitry Sinelnikov
 *  \date      2012
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "logger_impl.h"
// third-party
#include <boost/current_function.hpp>

#define LOG(level) \
   if (level < cs::logger::Log::GetLogLevel())\
      ;\
   else\
      (cs::logger::Log(level)) << cs::logger::Delimiter << BOOST_CURRENT_FUNCTION << cs::logger::Delimiter

/// log message with Debug level
#define LOGDBG		LOG(cs::logger::Debug)
/// log message with Warning log
#define LOGWRN		LOG(cs::logger::Warning)
/// log message with Error level
#define LOGERR		LOG(cs::logger::Error)
/// log message with Fatal level
#define LOGFTL		LOG(cs::logger::Fatal)
/// log Empty message (log always, no matter what log level is)
#define LOGEMPTY	(cs::logger::Log(cs::logger::Empty)) //

/// macros to set log level in runtime
#define SET_LOG_LEVEL(level)  cs::logger::Log::SetLogLevel((cs::logger::LevelId)level)

#endif // LOGGER_H
