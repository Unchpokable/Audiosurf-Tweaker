cmake_minimum_required(VERSION 3.20)

project(Tweaker.Native VERSION 1.0.0 LANGUAGES C CXX)

# Set C17 and C++20 standards
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    image_processing.c
    image_processing.h
    stb_image.h
    stb_image_write.h
)

# Create static library
add_library(Tweaker.Native STATIC ${SOURCES})

# Set target properties
set_target_properties(Tweaker.Native PROPERTIES
    C_STANDARD 17
    CXX_STANDARD 20
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(Tweaker.Native PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler-specific options
if(MSVC)
    target_compile_options(Tweaker.Native PRIVATE
        /W4
        /WX-
    )
    target_compile_definitions(Tweaker.Native PRIVATE
        _CRT_SECURE_NO_WARNINGS
        TWEAKER_NATIVE_EXPORT
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Tweaker.Native PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    target_compile_definitions(Tweaker.Native PRIVATE
        TWEAKER_NATIVE_EXPORT
    )
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(Tweaker.Native PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()