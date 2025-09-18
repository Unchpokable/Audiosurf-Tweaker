cmake_minimum_required(VERSION 3.16)

project(NativeImage LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(NativeImage SHARED


  image_processing.c image_processing.h stb_image_write.h stb_image.h
)

target_include_directories(NativeImage PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

target_compile_definitions(NativeImage PRIVATE NATIVEIMAGE_LIBRARY TWEAKER_NATIVE_EXPORT)
