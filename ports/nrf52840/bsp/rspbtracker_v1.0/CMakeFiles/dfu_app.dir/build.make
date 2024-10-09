# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0

# Utility rule file for dfu_app.

# Include any custom commands dependencies for this target.
include CMakeFiles/dfu_app.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/dfu_app.dir/progress.make

CMakeFiles/dfu_app:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Sending application image over DFU (reset device with DEBUG pin jumpered!)..."
	nrfutil dfu usb-serial -p /dev/ttyACM0 -pkg LinkIt_rspb_board_dfu.zip

dfu_app: CMakeFiles/dfu_app
dfu_app: CMakeFiles/dfu_app.dir/build.make
.PHONY : dfu_app

# Rule to build all files generated by this target.
CMakeFiles/dfu_app.dir/build: dfu_app
.PHONY : CMakeFiles/dfu_app.dir/build

CMakeFiles/dfu_app.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/dfu_app.dir/cmake_clean.cmake
.PHONY : CMakeFiles/dfu_app.dir/clean

CMakeFiles/dfu_app.dir/depend:
	cd /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840 /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840 /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0 /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0 /home/schade/Linkit/CLS-Argos-Linkit-CORE/ports/nrf52840/bsp/rspbtracker_v1.0/CMakeFiles/dfu_app.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/dfu_app.dir/depend
