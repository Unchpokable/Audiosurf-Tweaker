cmake_minimum_required(VERSION 3.16)

project(NativeImage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(NativeImage STATIC


  image_processing.c image_processing.h stb_image_write.h stb_image.h
)

target_compile_definitions(NativeImage PRIVATE NATIVEIMAGE_LIBRARY)
