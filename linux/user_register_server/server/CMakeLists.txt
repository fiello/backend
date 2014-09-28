project (server) 

set (SERVER_SOURCES 
  main.cpp
  )

set (SERVER_LIBRARIES
#  pthread
#  rt
  )

include_directories (
  ${PROJECT_SOURCE_DIR}
  ${COMMON_INCLUDE_DIRECTORIES}
  ${CMAKE_CURRENT_BINARY_DIR} # for forms headers 
  )

add_definitions (
  ${COMMON_DEFINITIONS}
  )  

source_group ("Source Files"          FILES ${SERVER_SOURCES})
 
add_executable (${server_OUTPUT} ${SERVER_SOURCES})
target_link_libraries (${server_OUTPUT} ${Boost_LIBRARIES} ${logger_OUTPUT} ${config_OUTPUT} ${ipc_OUTPUT} ${SERVER_LIBRARIES})
