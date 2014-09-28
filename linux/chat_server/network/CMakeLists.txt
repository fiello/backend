cmake_minimum_required (VERSION 2.8)

project (network CXX)

add_library (
   ${network_OUTPUT}
   STATIC
   network_manager.cc
   interface_addresses_holder.cc
   connection/connection_manager.cc
   connection/connection_holder.cc
   socket/socket_address_holder.cc
   socket/socket_wrapper.cc
)
   
target_link_libraries (
   ${network_OUTPUT}
   ${thread_pool_OUTPUT}
)
