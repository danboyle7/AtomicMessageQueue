# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.6

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
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
CMAKE_COMMAND = /opt/clion-2016.3.3/bin/cmake/bin/cmake

# The command to remove a file.
RM = /opt/clion-2016.3.3/bin/cmake/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/user/mq_libraries/ShmAtomicMsgQueue

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/ShmAtomicMsgQueue.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/ShmAtomicMsgQueue.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/ShmAtomicMsgQueue.dir/flags.make

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o: CMakeFiles/ShmAtomicMsgQueue.dir/flags.make
CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o"
	/opt/rh/devtoolset-3/root/usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o -c /home/user/mq_libraries/ShmAtomicMsgQueue/main.cpp

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.i"
	/opt/rh/devtoolset-3/root/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/user/mq_libraries/ShmAtomicMsgQueue/main.cpp > CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.i

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.s"
	/opt/rh/devtoolset-3/root/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/user/mq_libraries/ShmAtomicMsgQueue/main.cpp -o CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.s

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.requires:

.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.requires

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.provides: CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/ShmAtomicMsgQueue.dir/build.make CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.provides.build
.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.provides

CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.provides.build: CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o


# Object files for target ShmAtomicMsgQueue
ShmAtomicMsgQueue_OBJECTS = \
"CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o"

# External object files for target ShmAtomicMsgQueue
ShmAtomicMsgQueue_EXTERNAL_OBJECTS =

libShmAtomicMsgQueue.a: CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o
libShmAtomicMsgQueue.a: CMakeFiles/ShmAtomicMsgQueue.dir/build.make
libShmAtomicMsgQueue.a: CMakeFiles/ShmAtomicMsgQueue.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libShmAtomicMsgQueue.a"
	$(CMAKE_COMMAND) -P CMakeFiles/ShmAtomicMsgQueue.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ShmAtomicMsgQueue.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/ShmAtomicMsgQueue.dir/build: libShmAtomicMsgQueue.a

.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/build

CMakeFiles/ShmAtomicMsgQueue.dir/requires: CMakeFiles/ShmAtomicMsgQueue.dir/main.cpp.o.requires

.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/requires

CMakeFiles/ShmAtomicMsgQueue.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ShmAtomicMsgQueue.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/clean

CMakeFiles/ShmAtomicMsgQueue.dir/depend:
	cd /home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/user/mq_libraries/ShmAtomicMsgQueue /home/user/mq_libraries/ShmAtomicMsgQueue /home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug /home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug /home/user/mq_libraries/ShmAtomicMsgQueue/cmake-build-debug/CMakeFiles/ShmAtomicMsgQueue.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/ShmAtomicMsgQueue.dir/depend

