# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
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
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/runner/work/tarantool/tarantool

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/runner/work/tarantool/tarantool

# Utility rule file for man-tarantool.

# Include any custom commands dependencies for this target.
include doc/man/CMakeFiles/man-tarantool.dir/compiler_depend.make

# Include the progress variables for this target.
include doc/man/CMakeFiles/man-tarantool.dir/progress.make

doc/man/CMakeFiles/man-tarantool: doc/man/tarantool.1

doc/man/tarantool.1:
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/runner/work/tarantool/tarantool/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating tarantool.1"
	cd /home/runner/work/tarantool/tarantool/doc/man && /usr/bin/pod2man --section="1" --center="Lua\ application\ server\ and\ database\ management\ system" --release --name="tarantool" /home/runner/work/tarantool/tarantool/doc/man/tarantool.pod /home/runner/work/tarantool/tarantool/doc/man/tarantool.1

man-tarantool: doc/man/CMakeFiles/man-tarantool
man-tarantool: doc/man/tarantool.1
man-tarantool: doc/man/CMakeFiles/man-tarantool.dir/build.make
.PHONY : man-tarantool

# Rule to build all files generated by this target.
doc/man/CMakeFiles/man-tarantool.dir/build: man-tarantool
.PHONY : doc/man/CMakeFiles/man-tarantool.dir/build

doc/man/CMakeFiles/man-tarantool.dir/clean:
	cd /home/runner/work/tarantool/tarantool/doc/man && $(CMAKE_COMMAND) -P CMakeFiles/man-tarantool.dir/cmake_clean.cmake
.PHONY : doc/man/CMakeFiles/man-tarantool.dir/clean

doc/man/CMakeFiles/man-tarantool.dir/depend:
	cd /home/runner/work/tarantool/tarantool && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/runner/work/tarantool/tarantool /home/runner/work/tarantool/tarantool/doc/man /home/runner/work/tarantool/tarantool /home/runner/work/tarantool/tarantool/doc/man /home/runner/work/tarantool/tarantool/doc/man/CMakeFiles/man-tarantool.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : doc/man/CMakeFiles/man-tarantool.dir/depend

