cmake_minimum_required (VERSION 2.8)

project (ChatServer CXX)

set (project_ROOT ${PROJECT_SOURCE_DIR})
set (common_ROOT ${project_ROOT}/common)

if (WIN32)
   find_package (Boost REQUIRED)
else ()
   find_package (Boost COMPONENTS thread REQUIRED)
   find_package (Boost COMPONENTS date_time REQUIRED)
   find_package (Boost COMPONENTS program_options REQUIRED)
   find_package (Boost COMPONENTS regex REQUIRED)
endif ()

set (OUTPUT_DIRECTORY ${project_ROOT}/out_${CMAKE_SYSTEM_NAME})
set (EXECUTABLE_OUTPUT_PATH ${OUTPUT_DIRECTORY})
set (LIBRARY_OUTPUT_PATH ${OUTPUT_DIRECTORY})

set (COMMON_INCLUDE_DIRECTORIES
   ${project_ROOT}
   ${project_ROOT}/tools
   ${Boost_INCLUDE_DIRS}
)

set (COMMON_LINK_DIRECTORIES
   ${Boost_LIBRARY_DIRS}
   ${LIBRARY_OUTPUT_PATH}
)

include_directories (${COMMON_INCLUDE_DIRECTORIES})
link_directories (${COMMON_LINK_DIRECTORIES})

set (core_OUTPUT chat_server)
set (logger_OUTPUT logger)
set (config_OUTPUT config)
set (signal_OUTPUT signal)
set (thread_pool_OUTPUT thread_pool)
set (network_OUTPUT network)

set (project_VERSION_MAJOR 0)
set (project_VERSION_MINOR 6)
set (project_VERSION_BUILD 0)

set (COMMON_DEFINITIONS "-Wall -Werror -pedantic -Wno-long-long")
set (CMAKE_CXX_FLAGS_DEBUG "${COMMON_DEFINITIONS} -O0 -g -ggdb3 ")
set (CMAKE_CXX_FLAGS_RELEASE "${COMMON_DEFINITIONS} -O3 ")

configure_file (
   "${common_ROOT}/compiled_definitions.h.in"
   "${common_ROOT}/compiled_definitions.h"
)

add_subdirectory (core)
add_subdirectory (config)
add_subdirectory (signal)
add_subdirectory (network)
add_subdirectory (tools/logger)
add_subdirectory (tools/thread_pool)
