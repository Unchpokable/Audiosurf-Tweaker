cmake_minimum_required(VERSION 3.16)

project(Core LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(Core SHARED
  Core_global.h
  raw_image.hxx raw_image.cxx
  precompiled.hxx
  masks.hxx
  masks.cxx
  packager.hxx packager.cxx
  pack_data.hxx pack_data.cxx
  error.hxx error.cxx



)

target_precompile_headers(Core PRIVATE precompiled.hxx)

target_link_libraries(Core PUBLIC NativeImage)
target_link_libraries(Core PRIVATE Qt6::Core)

target_compile_definitions(Core PRIVATE CORE_LIBRARY)
