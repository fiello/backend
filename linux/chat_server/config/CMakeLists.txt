cmake_minimum_required (VERSION 2.8)

project (config CXX)

add_library (
   ${config_OUTPUT}
   STATIC
   configuration_manager.cc
)
target_link_libraries (${config_OUTPUT})
