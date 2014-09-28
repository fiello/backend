cmake_minimum_required (VERSION 2.8)

project (config)

set (CONFIG_HEADERS
  ${common_ROOT}/ConfigurationModule.h
  )

set (CONFIG_SOURCES
  ConfigurationModule.cpp
  )

include_directories (
  ${COMMON_INCLUDE_DIRECTORIES}
  ${PROJECT_SOURCE_DIR}
)
 
add_definitions (
  ${COMMON_DEFINITIONS}
)  

add_library(${config_OUTPUT} ${CONFIG_HEADERS} ${CONFIG_SOURCES})
target_link_libraries (${config_OUTPUT} ${Boost_LIBRARIES})
