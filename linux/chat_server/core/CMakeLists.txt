cmake_minimum_required (VERSION 2.8)

project (core CXX)
 
add_executable (
   ${core_OUTPUT}
   main.cc
   server_engine.cc
   data_processing/receive_data_task.cc
   data_processing/process_message_task.cc
   data_processing/write_answer_task.cc
)

target_link_libraries (
   ${core_OUTPUT}
   ${signal_OUTPUT}
   ${config_OUTPUT}
   ${logger_OUTPUT}
   ${network_OUTPUT}
   ${Boost_LIBRARIES}
)
