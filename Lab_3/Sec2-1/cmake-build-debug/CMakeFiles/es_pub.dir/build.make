# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

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
CMAKE_COMMAND = /var/lib/snapd/snap/clion/114/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /var/lib/snapd/snap/clion/114/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/es_pub.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/es_pub.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/es_pub.dir/flags.make

es.pb.cc: /home/edge/Code/RealTimeSystem/Lab_3/protos/es.proto
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating es.pb.cc, es.pb.h, es.grpc.pb.cc, es.grpc.pb.h"
	/bin/protoc-3.11.2.0 --grpc_out /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug --cpp_out /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug -I /home/edge/Code/RealTimeSystem/Lab_3/protos --plugin=protoc-gen-grpc="/bin/grpc_cpp_plugin" /home/edge/Code/RealTimeSystem/Lab_3/protos/es.proto

es.pb.h: es.pb.cc
	@$(CMAKE_COMMAND) -E touch_nocreate es.pb.h

es.grpc.pb.cc: es.pb.cc
	@$(CMAKE_COMMAND) -E touch_nocreate es.grpc.pb.cc

es.grpc.pb.h: es.pb.cc
	@$(CMAKE_COMMAND) -E touch_nocreate es.grpc.pb.h

CMakeFiles/es_pub.dir/es_pub.cc.o: CMakeFiles/es_pub.dir/flags.make
CMakeFiles/es_pub.dir/es_pub.cc.o: ../es_pub.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/es_pub.dir/es_pub.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/es_pub.dir/es_pub.cc.o -c /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/es_pub.cc

CMakeFiles/es_pub.dir/es_pub.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/es_pub.dir/es_pub.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/es_pub.cc > CMakeFiles/es_pub.dir/es_pub.cc.i

CMakeFiles/es_pub.dir/es_pub.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/es_pub.dir/es_pub.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/es_pub.cc -o CMakeFiles/es_pub.dir/es_pub.cc.s

CMakeFiles/es_pub.dir/es.pb.cc.o: CMakeFiles/es_pub.dir/flags.make
CMakeFiles/es_pub.dir/es.pb.cc.o: es.pb.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/es_pub.dir/es.pb.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/es_pub.dir/es.pb.cc.o -c /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.pb.cc

CMakeFiles/es_pub.dir/es.pb.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/es_pub.dir/es.pb.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.pb.cc > CMakeFiles/es_pub.dir/es.pb.cc.i

CMakeFiles/es_pub.dir/es.pb.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/es_pub.dir/es.pb.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.pb.cc -o CMakeFiles/es_pub.dir/es.pb.cc.s

CMakeFiles/es_pub.dir/es.grpc.pb.cc.o: CMakeFiles/es_pub.dir/flags.make
CMakeFiles/es_pub.dir/es.grpc.pb.cc.o: es.grpc.pb.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/es_pub.dir/es.grpc.pb.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/es_pub.dir/es.grpc.pb.cc.o -c /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.grpc.pb.cc

CMakeFiles/es_pub.dir/es.grpc.pb.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/es_pub.dir/es.grpc.pb.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.grpc.pb.cc > CMakeFiles/es_pub.dir/es.grpc.pb.cc.i

CMakeFiles/es_pub.dir/es.grpc.pb.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/es_pub.dir/es.grpc.pb.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/es.grpc.pb.cc -o CMakeFiles/es_pub.dir/es.grpc.pb.cc.s

# Object files for target es_pub
es_pub_OBJECTS = \
"CMakeFiles/es_pub.dir/es_pub.cc.o" \
"CMakeFiles/es_pub.dir/es.pb.cc.o" \
"CMakeFiles/es_pub.dir/es.grpc.pb.cc.o"

# External object files for target es_pub
es_pub_EXTERNAL_OBJECTS =

es_pub: CMakeFiles/es_pub.dir/es_pub.cc.o
es_pub: CMakeFiles/es_pub.dir/es.pb.cc.o
es_pub: CMakeFiles/es_pub.dir/es.grpc.pb.cc.o
es_pub: CMakeFiles/es_pub.dir/build.make
es_pub: /lib/libgrpc++_reflection.a
es_pub: /lib/libgrpc++.a
es_pub: /lib/libprotobuf.a
es_pub: /lib/libgrpc.a
es_pub: /lib/libssl.a
es_pub: /lib/libcrypto.a
es_pub: /lib/libgpr.a
es_pub: /lib/libabsl_str_format_internal.a
es_pub: /lib/libabsl_strings.a
es_pub: /lib/libabsl_throw_delegate.a
es_pub: /lib/libabsl_strings_internal.a
es_pub: /lib/libabsl_base.a
es_pub: /lib/libabsl_dynamic_annotations.a
es_pub: /lib/libabsl_spinlock_wait.a
es_pub: /usr/lib/librt.so
es_pub: /lib/libabsl_int128.a
es_pub: /lib/libabsl_bad_optional_access.a
es_pub: /lib/libabsl_raw_logging_internal.a
es_pub: /lib/libabsl_log_severity.a
es_pub: /lib/libupb.a
es_pub: /lib/libz.a
es_pub: /lib/libcares.a
es_pub: /lib/libaddress_sorting.a
es_pub: CMakeFiles/es_pub.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking CXX executable es_pub"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/es_pub.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/es_pub.dir/build: es_pub

.PHONY : CMakeFiles/es_pub.dir/build

CMakeFiles/es_pub.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/es_pub.dir/cmake_clean.cmake
.PHONY : CMakeFiles/es_pub.dir/clean

CMakeFiles/es_pub.dir/depend: es.pb.cc
CMakeFiles/es_pub.dir/depend: es.pb.h
CMakeFiles/es_pub.dir/depend: es.grpc.pb.cc
CMakeFiles/es_pub.dir/depend: es.grpc.pb.h
	cd /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1 /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1 /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug /home/edge/Code/RealTimeSystem/Lab_3/Sec2-1/cmake-build-debug/CMakeFiles/es_pub.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/es_pub.dir/depend

