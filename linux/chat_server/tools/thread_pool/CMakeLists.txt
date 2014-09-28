cmake_minimum_required (VERSION 2.8)

project (thread_pool CXX)

add_library (
   ${thread_pool_OUTPUT} 
   STATIC 
   thread_pool.cc
)

target_link_libraries (${thread_pool_OUTPUT})
