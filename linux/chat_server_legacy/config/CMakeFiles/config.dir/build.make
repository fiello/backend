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
include config/CMakeFiles/config.dir/depend.make

# Include the progress variables for this target.
include config/CMakeFiles/config.dir/progress.make

# Include the compile flags for this target's objects.
include config/CMakeFiles/config.dir/flags.make

config/CMakeFiles/config.dir/ConfigurationModule.cpp.o: config/CMakeFiles/config.dir/flags.make
config/CMakeFiles/config.dir/ConfigurationModule.cpp.o: config/ConfigurationModule.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vdi/SVN/Klarna_ChatServer/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object config/CMakeFiles/config.dir/ConfigurationModule.cpp.o"
	cd /home/vdi/SVN/Klarna_ChatServer/config && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/config.dir/ConfigurationModule.cpp.o -c /home/vdi/SVN/Klarna_ChatServer/config/ConfigurationModule.cpp

config/CMakeFiles/config.dir/ConfigurationModule.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/config.dir/ConfigurationModule.cpp.i"
	cd /home/vdi/SVN/Klarna_ChatServer/config && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/vdi/SVN/Klarna_ChatServer/config/ConfigurationModule.cpp > CMakeFiles/config.dir/ConfigurationModule.cpp.i

config/CMakeFiles/config.dir/ConfigurationModule.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/config.dir/ConfigurationModule.cpp.s"
	cd /home/vdi/SVN/Klarna_ChatServer/config && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/vdi/SVN/Klarna_ChatServer/config/ConfigurationModule.cpp -o CMakeFiles/config.dir/ConfigurationModule.cpp.s

config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.requires:
.PHONY : config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.requires

config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.provides: config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.requires
	$(MAKE) -f config/CMakeFiles/config.dir/build.make config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.provides.build
.PHONY : config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.provides

config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.provides.build: config/CMakeFiles/config.dir/ConfigurationModule.cpp.o

# Object files for target config
config_OBJECTS = \
"CMakeFiles/config.dir/ConfigurationModule.cpp.o"

# External object files for target config
config_EXTERNAL_OBJECTS =

config/libconfig.a: config/CMakeFiles/config.dir/ConfigurationModule.cpp.o
config/libconfig.a: config/CMakeFiles/config.dir/build.make
config/libconfig.a: config/CMakeFiles/config.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library libconfig.a"
	cd /home/vdi/SVN/Klarna_ChatServer/config && $(CMAKE_COMMAND) -P CMakeFiles/config.dir/cmake_clean_target.cmake
	cd /home/vdi/SVN/Klarna_ChatServer/config && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/config.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
config/CMakeFiles/config.dir/build: config/libconfig.a
.PHONY : config/CMakeFiles/config.dir/build

config/CMakeFiles/config.dir/requires: config/CMakeFiles/config.dir/ConfigurationModule.cpp.o.requires
.PHONY : config/CMakeFiles/config.dir/requires

config/CMakeFiles/config.dir/clean:
	cd /home/vdi/SVN/Klarna_ChatServer/config && $(CMAKE_COMMAND) -P CMakeFiles/config.dir/cmake_clean.cmake
.PHONY : config/CMakeFiles/config.dir/clean

config/CMakeFiles/config.dir/depend:
	cd /home/vdi/SVN/Klarna_ChatServer && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/config /home/vdi/SVN/Klarna_ChatServer /home/vdi/SVN/Klarna_ChatServer/config /home/vdi/SVN/Klarna_ChatServer/config/CMakeFiles/config.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : config/CMakeFiles/config.dir/depend

