# Top level CMakeLists.txt for UserRegisterServer project
cmake_minimum_required (VERSION 2.8)

# Project name
project( UserRegisterServer )

set (server_VERSION_MAJOR 0) # 0.15.0 = (not released, 14 + several patches/fixes)
set (server_VERSION_MINOR 15)
set (server_VERSION_PATCH 0)

# Init cmake
set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/")

# Configuring Boost libraries: static linkage
set(Boost_USE_STATIC_LIBS     ON)
set(Boost_USE_MULTITHREADED   ON)
set(Boost_USE_STATIC_RUNTIME  OFF)

# Necessary packages
find_package (Boost COMPONENTS thread REQUIRED)
find_package (Boost COMPONENTS system REQUIRED)
find_package (Boost COMPONENTS filesystem REQUIRED)
find_package (Boost COMPONENTS program_options REQUIRED)

set (LINK_SEARCH_END_STATIC TRUE)
set (CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

# important paths
set (server_ROOT ${PROJECT_SOURCE_DIR}/server)
set (common_ROOT ${PROJECT_SOURCE_DIR}/common)
set (logger_ROOT ${PROJECT_SOURCE_DIR}/logger)
set (config_ROOT ${PROJECT_SOURCE_DIR}/config)
set (ipc_ROOT ${PROJECT_SOURCE_DIR}/ipc)
set (external_ROOT ${PROJECT_SOURCE_DIR}/external)

# Set compilation params
set (COMMON_INCLUDE_DIRECTORIES
#  ${server_ROOT}
  ${common_ROOT}
  ${external_ROOT}
  ${ipc_ROOT}
  ${Boost_INCLUDE_DIRS}
  )

# Common debug definitions to add in debug builds
set (COMMON_DEFINITIONS
    -Wall
    -Werror
    -pedantic
    -Wno-long-long
    -Wno-variadic-macros
    -std=c++0x
  )

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set (COMMON_DEFINITIONS
    ${COMMON_DEFINITIONS}  
    -DDEBUG
    -D_DEBUG
  )  
endif()

# Naming targets
set (server_OUTPUT user-reg-server)
set (logger_OUTPUT logger)
set (config_OUTPUT config)
set (ipc_OUTPUT ipc)

# generate header with some definitions
configure_file (
  "${common_ROOT}/CompiledDefinitions.h.in"
  "${common_ROOT}/CompiledDefinitions.h"
  )

# Run in those subdirectories
add_subdirectory( server )
add_subdirectory( logger )
add_subdirectory( config )
add_subdirectory( ipc )
