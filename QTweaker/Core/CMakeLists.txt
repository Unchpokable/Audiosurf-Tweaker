cmake_minimum_required(VERSION 3.16)

project(Core LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Concurrent)

find_package(libzip CONFIG REQUIRED)

if(WIN32)
find_package(xxHash CONFIG REQUIRED)
else()
find_package(PkgConfig REQUIRED)
pkg_check_modules(xxHash REQUIRED IMPORTED_TARGET libxxhash)
endif()

add_library(Core SHARED
  Core_global.h
  raw_image.hxx raw_image.cxx
  precompiled.hxx
  masks.hxx masks.cxx
  packager.hxx packager.cxx
  pack_data.hxx pack_data.cxx
  error.hxx error.cxx
  zip_support.hxx zip_support.cxx
  zip_handle.hxx zip_handle.cxx
  logging.hxx logging.cxx
  installer.hxx installer.cxx
  snapshot.hxx snapshot.cxx
)

target_precompile_headers(Core PRIVATE precompiled.hxx)

target_link_libraries(Core PUBLIC NativeImage)
target_link_libraries(Core PRIVATE Qt6::Core)
target_link_libraries(Core PRIVATE Qt6::Concurrent)
target_link_libraries(Core PRIVATE libzip::zip)

if(WIN32)
target_link_libraries(Core PRIVATE xxHash::xxhash)
else()
target_link_libraries(Core PRIVATE PkgConfig::xxHash)
endif()

target_include_directories(Core PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

target_compile_definitions(Core PRIVATE CORE_LIBRARY)
