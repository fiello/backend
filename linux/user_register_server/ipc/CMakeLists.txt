cmake_minimum_required (VERSION 2.8)

project (ipc)

set (IPC_HEADERS
  ${common_ROOT}/IPCModule.h
  ProcessData.h
  ThreadPoolModule.h
  )

set (IPC_SOURCES
  IPCModule.cpp
  ProcessData.cpp
  ThreadPoolModule.cpp
  )

set (IPC_LIBRARIES
  pthread
  rt
  )

include_directories (
  ${COMMON_INCLUDE_DIRECTORIES}
  ${PROJECT_SOURCE_DIR}
)
 
add_definitions (
  ${COMMON_DEFINITIONS}
)  

add_library(${ipc_OUTPUT} ${IPC_HEADERS} ${IPC_SOURCES})
target_link_libraries (${ipc_OUTPUT} ${Boost_LIBRARIES} ${IPC_LIBRARIES})
