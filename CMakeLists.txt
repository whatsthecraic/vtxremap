cmake_minimum_required(VERSION 3.14)

set(CMAKE_VERBOSE_MAKEFILE ON) # Be verbose
set(CMAKE_C_COMPILER_NAMES clang gcc icc cc) # Prefer Clang over GCC
set(CMAKE_CXX_COMPILER_NAMES clang++ g++ icpc c++ cxx) # Prefer Clang over GCC
set(CMAKE_CXX_STANDARD 17)

project(graphalytics_vertex_remap)

# Search for the project modules in /build-aux/
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build-aux/")

# Helpers
include(AppendCompilerFlag)
include(GetCompilerArgs)
include(Trim)

# Default build type
# Based on https://blog.kitware.com/cmake-and-the-default-build-type/
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to `DEBUG' as none was specified.")
    set(CMAKE_BUILD_TYPE "DEBUG" CACHE STRING "Choose the type of build." FORCE)
    set(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

# Emit full debug info in clang
get_c_compiler_flags("" c_flags)
get_cxx_compiler_flags("" cxx_flags)
if("${c_flags}" MATCHES "( |^)-g( |$)")
    append_compiler_flag("-fno-omit-frame-pointer" "C")
    append_compiler_flag("-fno-limit-debug-info" "C")
endif()
if("${cxx_flags}" MATCHES "( |^)-g( |$)")
    append_compiler_flag("-fno-omit-frame-pointer" "CXX")
    append_compiler_flag("-fno-limit-debug-info" "CXX")
endif()

find_package(ZLIB)

# Create the list of objects
add_subdirectory(lib/common)

add_executable(vtxremap
    lib/cxxopts.hpp
    edge.cpp edge.hpp
    graphalytics_algorithms.cpp graphalytics_algorithms.hpp
    graphalytics_reader.cpp graphalytics_reader.hpp
    main.cpp
)

target_link_libraries(vtxremap PUBLIC libcommon)
target_link_libraries(vtxremap PUBLIC ZLIB::ZLIB)

get_c_compiler_flags(vtxremap c_flags)
get_cxx_compiler_flags(vtxremap cxx_flags)
message("Compiler C..........: ${CMAKE_C_COMPILER} ${c_flags}")
message("Compiler C++........: ${CMAKE_CXX_COMPILER} ${cxx_flags}")