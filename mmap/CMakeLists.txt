cmake_minimum_required(VERSION 3.16)

project(mmap VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Architecture option
set(MMAP_ARCHITECTURE "x64" CACHE STRING "Target architecture (x32 or x64)")
set_property(CACHE MMAP_ARCHITECTURE PROPERTY STRINGS "x32" "x64")

# Validate architecture option
if(NOT MMAP_ARCHITECTURE MATCHES "^(x32|x64)$")
    message(FATAL_ERROR "MMAP_ARCHITECTURE must be either 'x32' or 'x64', got: ${MMAP_ARCHITECTURE}")
endif()

# Set architecture-specific flags
if(MMAP_ARCHITECTURE STREQUAL "x32")
    set(MMAP_MACHINE_32 1)
    set(MMAP_MACHINE_64 0)

    if(MSVC)
        # For x32 on MSVC, use Win32 generator or don't set /arch flag
        # /arch:IA32 is not a valid flag
        if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
            message(WARNING "For x32 build with MSVC, configure with: cmake -A Win32")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    endif()

    message(STATUS "Building for x32 architecture")
else()
    set(MMAP_MACHINE_32 0)
    set(MMAP_MACHINE_64 1)

    if(MSVC)
        # For x64, we can optionally use AVX (or leave default)
        # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
        if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
            message(STATUS "For x64 build with MSVC, you can configure with: cmake -A x64")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
    endif()

    message(STATUS "Building for x64 architecture")
endif()

# Generate config.h
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

# Make config.h available to subdirectories
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_compile_definitions(_MBCS)

# Disable default exception handling flags for MSVC
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Add source subdirectory
add_subdirectory(src)
