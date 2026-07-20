# The Quest3D SDK is proprietary and is never committed to this repository
# (see Quest3D-SDK/ which is gitignored). Point Q3D_SDK_DIR at a local copy
# of it, either via -DQ3D_SDK_DIR=... or the QUEST3D_SDK_DIR environment
# variable. Defaults to the conventional in-tree location, next to
# DirectX-SDK-June2010/ and Detours/.

if(DEFINED ENV{QUEST3D_SDK_DIR})
    set(_q3d_sdk_default "$ENV{QUEST3D_SDK_DIR}")
else()
    set(_q3d_sdk_default "${CMAKE_SOURCE_DIR}/Quest3D-SDK")
endif()

set(Q3D_SDK_DIR "${_q3d_sdk_default}" CACHE PATH
    "Path to a local copy of the Quest3D SDK (must contain include/A3d_EngineInterface.h)")

if(NOT EXISTS "${Q3D_SDK_DIR}/include/A3d_EngineInterface.h")
    message(FATAL_ERROR
        "Quest3D SDK not found at \"${Q3D_SDK_DIR}\".\n"
        "This SDK is proprietary and is not distributed with this repository.\n"
        "Obtain it yourself and point CMake at it, e.g.:\n"
        "  cmake --preset x86-release -DQ3D_SDK_DIR=\"C:/path/to/q3d SDK\"\n"
        "or set the QUEST3D_SDK_DIR environment variable before configuring.")
endif()

add_library(quest3d_sdk INTERFACE)
target_include_directories(quest3d_sdk INTERFACE "${Q3D_SDK_DIR}/include")
