# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.24

# Default target executed when no arguments are given to make.
default_target: all
.PHONY : default_target

# Allow only one "make -f Makefile2" at a time, but pass parallelism.
.NOTPARALLEL:

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
CMAKE_COMMAND = /opt/local/bin/cmake

# The command to remove a file.
RM = /opt/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/florian/Downloads/modbus_proxy-main/c

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/florian/Downloads/modbus_proxy-main/c

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake cache editor..."
	/opt/local/bin/ccmake -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache
.PHONY : edit_cache/fast

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/opt/local/bin/cmake --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache
.PHONY : rebuild_cache/fast

# The main all target
all: cmake_check_build_system
	$(CMAKE_COMMAND) -E cmake_progress_start /Users/florian/Downloads/modbus_proxy-main/c/CMakeFiles /Users/florian/Downloads/modbus_proxy-main/c//CMakeFiles/progress.marks
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 all
	$(CMAKE_COMMAND) -E cmake_progress_start /Users/florian/Downloads/modbus_proxy-main/c/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 clean
.PHONY : clean

# The main clean target
clean/fast: clean
.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

#=============================================================================
# Target rules for targets named modbus_proxy

# Build rule for target.
modbus_proxy: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 modbus_proxy
.PHONY : modbus_proxy

# fast build rule for target.
modbus_proxy/fast:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/modbus_proxy.dir/build.make CMakeFiles/modbus_proxy.dir/build
.PHONY : modbus_proxy/fast

#=============================================================================
# Target rules for targets named logging

# Build rule for target.
logging: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 logging
.PHONY : logging

# fast build rule for target.
logging/fast:
	$(MAKE) $(MAKESILENT) -f components/logging/CMakeFiles/logging.dir/build.make components/logging/CMakeFiles/logging.dir/build
.PHONY : logging/fast

#=============================================================================
# Target rules for targets named modbus_tcp_client

# Build rule for target.
modbus_tcp_client: cmake_check_build_system
	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 modbus_tcp_client
.PHONY : modbus_tcp_client

# fast build rule for target.
modbus_tcp_client/fast:
	$(MAKE) $(MAKESILENT) -f components/modbus_tcp_client/CMakeFiles/modbus_tcp_client.dir/build.make components/modbus_tcp_client/CMakeFiles/modbus_tcp_client.dir/build
.PHONY : modbus_tcp_client/fast

main/main.o: main/main.c.o
.PHONY : main/main.o

# target to build an object file
main/main.c.o:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/modbus_proxy.dir/build.make CMakeFiles/modbus_proxy.dir/main/main.c.o
.PHONY : main/main.c.o

main/main.i: main/main.c.i
.PHONY : main/main.i

# target to preprocess a source file
main/main.c.i:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/modbus_proxy.dir/build.make CMakeFiles/modbus_proxy.dir/main/main.c.i
.PHONY : main/main.c.i

main/main.s: main/main.c.s
.PHONY : main/main.s

# target to generate assembly for a file
main/main.c.s:
	$(MAKE) $(MAKESILENT) -f CMakeFiles/modbus_proxy.dir/build.make CMakeFiles/modbus_proxy.dir/main/main.c.s
.PHONY : main/main.c.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... logging"
	@echo "... modbus_proxy"
	@echo "... modbus_tcp_client"
	@echo "... main/main.o"
	@echo "... main/main.i"
	@echo "... main/main.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

