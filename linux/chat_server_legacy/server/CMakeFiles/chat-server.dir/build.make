# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/local/bin/cmake-gui

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vdi/SVN/Klarna_ChatServer

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vdi/SVN/Klarna_ChatServer

# Include any dependencies generated for this target.
include server/CMakeFiles/chat-server.dir/depend.make

# Include the progress variables for this target.
include server/CMakeFiles/chat-server.dir/progress.make

# Include the compile flags for this target's objects.
include server/CMakeFiles/chat-server.dir/flags.make

server/CMakeFiles/chat-server.dir/main.cpp.o: server/CMakeFiles/chat-server.dir/flags.make
server/CMakeFiles/chat-server.dir/main.cpp.o: server/main.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vdi/SVN/Klarna_ChatServer/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object server/CMakeFiles/chat-server.dir/main.cpp.o"
	cd /home/vdi/SVN/Klarna_ChatServer/server && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/chat-server.dir/main.cpp.o -c /home/vdi/SVN/Klarna_ChatServer/server/main.cpp

server/CMakeFiles/chat-server.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/chat-server.dir/main.cpp.i"
	cd /home/vdi/SVN/Klarna_ChatServer/server && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/vdi/SVN/Klarna_ChatServer/server/main.cpp > CMakeFiles/chat-server.dir/main.cpp.i

server/CMakeFiles/chat-server.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/chat-server.dir/main.cpp.s"
	cd /home/vdi/SVN/Klarna_ChatServer/server && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/vdi/SVN/Klarna_ChatServer/server/main.cpp -o CMakeFiles/chat-server.dir/main.cpp.s

server/CMakeFiles/chat-server.dir/main.cpp.o.requires:
.PHONY : server/CMakeFiles/chat-server.dir/main.cpp.o.requires

server/CMakeFiles/chat-server.dir/main.cpp.o.provides: server/CMakeFiles/chat-server.dir/main.cpp.o.requires
	$(MAKE) -f server/CMakeFiles/chat-server.dir/build.make server/CMakeFiles/chat-server.dir/main.cpp.o.provides.build
.PHONY : server/CMakeFiles/chat-server.dir/main.cpp.o.provides

server/CMakeFiles/chat-server.dir/main.cpp.o.provides.build: server/CMakeFiles/chat-server.dir/main.cpp.o

# Object files for target chat-server
chat__server_OBJECTS = \
"CMakeFiles/chat-server.dir/main.cpp.o"

# External object files for target chat-server
chat__server_EXTERNAL_OBJECTS =

server/chat-server: server/CMakeFiles/chat-server.dir/main.cpp.o
server/chat-server: /usr/local/lib/libboost_thread.a
server/chat-server: /usr/local/lib/libboost_system.a
server/chat-server: /usr/local/lib/libboost_filesystem.a
server/chat-server: /usr/local/lib/libboost_program_options.a
server/chat-server: logger/liblogger.a
server/chat-server: config/libconfig.a
server/chat-server: ipc/libipc.a
server/chat-server: /usr/local/lib/libboost_thread.a
server/chat-server: /usr/local/lib/libboost_system.a
server/chat-server: /usr/local/lib/libboost_filesystem.a
server/chat-server: /usr/local/lib/libboost_program_options.a
server/chat-server: server/CMakeFiles/chat-server.dir/build.make
server/chat-server: server/CMakeFiles/chat-server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable chat-server"
	cd /home/vdi/SVN/Klarna_ChatServer/server && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/chat-server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
server/CMakeFiles/chat-server.dir/build: server/chat-server
.PHONY : server/CMakeFiles/chat-server.dir/build

server/CMakeFiles/chat-server.dir/requires: server/CMakeFiles/chat-server.dir/main.cpp.o.requires
.PHONY : server/CMakeFiles/chat-server.dir/requires

server/CMakeFiles/chat-server.dir/clean:
	cd /home/vdi/SVN/Klarna_ChatServer/server && $(CMAKE_COMMAND) -P CMakeFiles/chat-server.dir/cmake_clean.cmake
.PHONY : server/CMakeFiles/chat-server.dir/clean

server/CMakeFiles/chat-server.dir/depend:
	cd /home/vdi/SVN/Klarna_ChatServer && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/server /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/server /home/vdi/SVN/Klarna_ChatServer/server/CMakeFiles/chat-server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : server/CMakeFiles/chat-server.dir/depend

