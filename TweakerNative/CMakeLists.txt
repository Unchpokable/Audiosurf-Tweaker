cmake_minimum_required(VERSION 3.24)
project(TweakerNative LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "TweakerNative is a Windows x64 host library; configure with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)

# Official release archives, including TagLib's bundled utfcpp submodule. No vendor edits.
FetchContent_Declare(zlib
    URL https://zlib.net/fossils/zlib-1.4.tar.gz
    URL_HASH SHA256=02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)
add_library(ZLIB::ZLIB ALIAS zlibstatic)
set(ZLIB_LIBRARY zlibstatic CACHE STRING "Pinned static zlib target" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}" CACHE PATH "Pinned zlib headers" FORCE)

FetchContent_Declare(taglib
    URL https://taglib.org/releases/taglib-2.3.2.tar.gz
    URL_HASH SHA256=3ca2d8afaa7f1cf7f6ed10e511ebc368bfacd6dcaa3dbfa690b89e502e8963dc
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
set(WITH_ZLIB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(taglib)
get_directory_property(TAGLIB_HAVE_ZLIB DIRECTORY "${taglib_SOURCE_DIR}" DEFINITION HAVE_ZLIB)
if(NOT TAGLIB_HAVE_ZLIB)
    message(FATAL_ERROR "TagLib must retain support for compressed ID3 frames via zlib.")
endif()

add_library(TweakerNative SHARED metadata.cxx metadata.hxx)
# Upstream exposes installed include paths only; use its build-tree directories here.
target_include_directories(TweakerNative PRIVATE
    "${taglib_SOURCE_DIR}/taglib" "${taglib_SOURCE_DIR}/taglib/toolkit"
    "${taglib_SOURCE_DIR}/taglib/flac" "${taglib_SOURCE_DIR}/taglib/mp4"
    "${taglib_SOURCE_DIR}/taglib/riff/wav" "${taglib_BINARY_DIR}")
target_link_libraries(TweakerNative PRIVATE tag)
target_compile_options(TweakerNative PRIVATE /W4 /utf-8)
set_target_properties(TweakerNative PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# These travel with the host library, including the embedded header-only utfcpp dependency.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/licenses")
configure_file("${taglib_SOURCE_DIR}/COPYING.MPL" "licenses/TagLib-MPL.txt" COPYONLY)
configure_file("${taglib_SOURCE_DIR}/COPYING.LGPL" "licenses/TagLib-LGPL.txt" COPYONLY)
configure_file("${taglib_SOURCE_DIR}/3rdparty/utfcpp/LICENSE" "licenses/utfcpp.txt" COPYONLY)
configure_file("${zlib_SOURCE_DIR}/LICENSE" "licenses/zlib.txt" COPYONLY)
