cmake_minimum_required (VERSION 2.8)

project (signal CXX)

add_library (
   ${signal_OUTPUT} 
   STATIC 
   signal_manager.cc
)
target_link_libraries (${signal_OUTPUT})
