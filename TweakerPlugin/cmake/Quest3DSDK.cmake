# The Quest3D SDK is proprietary and is never committed to the public tree.
# Two supported layouts:
#   - a local copy at Quest3D-SDK/ (gitignored), the development machine;
#   - a private organization submodule at the same path with `update = none`,
#     which only a workflow with a token can check out (Docs/Internal/repo-layout-plan.md
#     and Tools/ReleaseChannel/README.md).
# Point Q3D_SDK_DIR at another copy via -DQ3D_SDK_DIR=... or the
# QUEST3D_SDK_DIR environment variable. Defaults to the in-tree location,
# next to DirectX-SDK-June2010/ and Detours/.

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
