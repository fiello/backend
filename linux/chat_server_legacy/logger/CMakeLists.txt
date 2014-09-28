cmake_minimum_required (VERSION 2.8)

project (logger)

set (LOGGER_HEADERS
  ${common_ROOT}/LogHelper.h
  ${common_ROOT}/Logger.h
  )

set (LOGGER_SOURCES
  Logger.cpp
  )

include_directories (
  ${COMMON_INCLUDE_DIRECTORIES}
  ${PROJECT_SOURCE_DIR}
)
 
add_definitions (
  ${COMMON_DEFINITIONS}
)  

add_library(${logger_OUTPUT} ${LOGGER_HEADERS} ${LOGGER_SOURCES})
target_link_libraries (${logger_OUTPUT} ${Boost_LIBRARIES})
