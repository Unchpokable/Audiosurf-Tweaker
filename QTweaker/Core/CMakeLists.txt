cmake_minimum_required(VERSION 3.16)

project(Core LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(Core SHARED
  Core_global.h
  raw_image.hxx raw_image.cxx image_formats.hxx
  precompiled.hxx
  masks.hxx
  masks.cxx
  packager.h packager.cpp
  pack_data.h pack_data.cpp



)

target_precompile_headers(Core PRIVATE precompiled.hxx)

target_link_libraries(Core PRIVATE Qt${QT_VERSION_MAJOR}::Core NativeImage)
target_link_libraries(Core PRIVATE Qt6::Core)
target_link_libraries(Core PRIVATE Qt6::Core)

target_include_directories(Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NativeImage)

target_compile_definitions(Core PRIVATE CORE_LIBRARY)
