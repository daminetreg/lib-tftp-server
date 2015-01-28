cmake_minimum_required(VERSION 2.8.8)
project (lib-tftp-server)

set (CMAKE_EXPORT_COMPILE_COMMANDS 1) # For .ycm_extra_conf.py
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 --std=c++11 -Wall -Werror")

set (CMAKE_BUILD_TYPE "DEBUG")

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -gdwarf-2") # debug symbols compatible with C++11 lambdas
endif (CMAKE_BUILD_TYPE STREQUAL "DEBUG")

# Versioning
set (lib_tftp_server_VERSION_MAJOR 1)
set (lib_tftp_server_VERSION_MINOR 0)
set (lib_tftp_server_VERSION_PATCH 0)

add_subdirectory(src)
