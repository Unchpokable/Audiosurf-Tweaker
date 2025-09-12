cmake_minimum_required(VERSION 3.16)

project(Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Core STATIC
  raw_image.cxx
  raw_image.hxx
)

target_compile_definitions(Core PRIVATE CORE_LIBRARY)
