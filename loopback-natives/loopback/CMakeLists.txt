cmake_minimum_required(VERSION 3.0)
project(loopback CXX)

include_directories("$ENV{JAVA_HOME}/include/")
include_directories("$ENV{JAVA_HOME}/include/win32")

add_definitions(-Dinline=__inline)
set(CMAKE_C_FLAGS_RELEASE "/MT")

if (DEFINED ENV{DIST_DIR})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{DIST_DIR}")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "$ENV{DIST_DIR}")
endif()

add_library(loopback SHARED loopback.cpp)
