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
include logger/CMakeFiles/logger.dir/depend.make

# Include the progress variables for this target.
include logger/CMakeFiles/logger.dir/progress.make

# Include the compile flags for this target's objects.
include logger/CMakeFiles/logger.dir/flags.make

logger/CMakeFiles/logger.dir/Logger.cpp.o: logger/CMakeFiles/logger.dir/flags.make
logger/CMakeFiles/logger.dir/Logger.cpp.o: logger/Logger.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vdi/SVN/Klarna_ChatServer/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object logger/CMakeFiles/logger.dir/Logger.cpp.o"
	cd /home/vdi/SVN/Klarna_ChatServer/logger && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/logger.dir/Logger.cpp.o -c /home/vdi/SVN/Klarna_ChatServer/logger/Logger.cpp

logger/CMakeFiles/logger.dir/Logger.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/logger.dir/Logger.cpp.i"
	cd /home/vdi/SVN/Klarna_ChatServer/logger && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/vdi/SVN/Klarna_ChatServer/logger/Logger.cpp > CMakeFiles/logger.dir/Logger.cpp.i

logger/CMakeFiles/logger.dir/Logger.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/logger.dir/Logger.cpp.s"
	cd /home/vdi/SVN/Klarna_ChatServer/logger && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/vdi/SVN/Klarna_ChatServer/logger/Logger.cpp -o CMakeFiles/logger.dir/Logger.cpp.s

logger/CMakeFiles/logger.dir/Logger.cpp.o.requires:
.PHONY : logger/CMakeFiles/logger.dir/Logger.cpp.o.requires

logger/CMakeFiles/logger.dir/Logger.cpp.o.provides: logger/CMakeFiles/logger.dir/Logger.cpp.o.requires
	$(MAKE) -f logger/CMakeFiles/logger.dir/build.make logger/CMakeFiles/logger.dir/Logger.cpp.o.provides.build
.PHONY : logger/CMakeFiles/logger.dir/Logger.cpp.o.provides

logger/CMakeFiles/logger.dir/Logger.cpp.o.provides.build: logger/CMakeFiles/logger.dir/Logger.cpp.o

# Object files for target logger
logger_OBJECTS = \
"CMakeFiles/logger.dir/Logger.cpp.o"

# External object files for target logger
logger_EXTERNAL_OBJECTS =

logger/liblogger.a: logger/CMakeFiles/logger.dir/Logger.cpp.o
logger/liblogger.a: logger/CMakeFiles/logger.dir/build.make
logger/liblogger.a: logger/CMakeFiles/logger.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library liblogger.a"
	cd /home/vdi/SVN/Klarna_ChatServer/logger && $(CMAKE_COMMAND) -P CMakeFiles/logger.dir/cmake_clean_target.cmake
	cd /home/vdi/SVN/Klarna_ChatServer/logger && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/logger.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
logger/CMakeFiles/logger.dir/build: logger/liblogger.a
.PHONY : logger/CMakeFiles/logger.dir/build

logger/CMakeFiles/logger.dir/requires: logger/CMakeFiles/logger.dir/Logger.cpp.o.requires
.PHONY : logger/CMakeFiles/logger.dir/requires

logger/CMakeFiles/logger.dir/clean:
	cd /home/vdi/SVN/Klarna_ChatServer/logger && $(CMAKE_COMMAND) -P CMakeFiles/logger.dir/cmake_clean.cmake
.PHONY : logger/CMakeFiles/logger.dir/clean

logger/CMakeFiles/logger.dir/depend:
	cd /home/vdi/SVN/Klarna_ChatServer && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/logger /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/logger /home/vdi/SVN/Klarna_ChatServer/logger/CMakeFiles/logger.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : logger/CMakeFiles/logger.dir/depend

