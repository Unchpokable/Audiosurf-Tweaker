cmake_minimum_required(VERSION 3.21)

project(TweakerPlugin LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "TweakerPlugin is Windows-only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "TweakerPlugin targets x86 only: Audiosurf is a 32-bit process, so a "
        "64-bit build of this DLL could never be injected into it. Configure "
        "with a 32-bit toolset (e.g. the x86-* CMake presets, or run from an "
        "\"x86 Native Tools Command Prompt for VS\").")
endif()

# An empty CMAKE_BUILD_TYPE on a single-config generator means "no optimization flags at all", which
# MSVC then treats much like a debug build - and nothing says so. That is not a hypothetical: the
# build type lives only in the preset, so if CMake ever decides the cache must be deleted mid-build
# (it does that when the resolved compiler path changes, e.g. because the shell was set up with
# `vcvarsall x86` on one run and `vcvarsall amd64_x86` on the next), the re-configure it does on its
# own has no preset behind it, and `--build --preset x86-release` quietly starts producing
# unoptimized binaries into a directory named "release".
#
# So: refuse instead. The fix is always `cmake --preset <name>` from a correctly set up shell -
# these presets declare `architecture x86` / `toolset host=x64` with strategy "external", which means
# the environment has to be an **amd64_x86** one.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR
        "CMAKE_BUILD_TYPE is empty on a single-config generator - the build would be unoptimized "
        "no matter what this directory is called. Reconfigure from the preset:\n"
        "    cmake --preset x86-debug     (or x86-release)\n"
        "from a shell prepared with `vcvarsall.bat amd64_x86`. If this appeared out of nowhere, "
        "CMake deleted the cache because the compiler path changed - delete the build directory and "
        "configure again.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_compile_definitions(UNICODE _UNICODE)

include(cmake/Detours.cmake)
include(cmake/DirectXSDK.cmake)
include(cmake/Quest3DSDK.cmake)
include(cmake/ImGui.cmake)
include(cmake/LunaSVG.cmake)
include(cmake/LuaJIT.cmake)

add_library(TweakerPlugin SHARED)
add_subdirectory(src)
add_subdirectory(smoke)
add_subdirectory(probe)

target_include_directories(TweakerPlugin PRIVATE src)

target_link_libraries(TweakerPlugin PRIVATE
    detours
    dxsdk
    quest3d_sdk
    imgui
    luajit
)

target_precompile_headers(TweakerPlugin PRIVATE src/pch.hxx)

target_link_options(TweakerPlugin PRIVATE /STACK:4194304)

# The plugin's version: the VERSIONINFO resource the host reads off the DLL on disk, and the same string
# in the lifecycle log and, later, in HANDSHAKE_ACK (Docs/Internal/plugin-offline-mode.md, Р-13). The
# build number is for whoever builds a release to set; a developer build is .0.
set(TWEAKER_PLUGIN_VERSION "0.1.0" CACHE STRING "TweakerPlugin version, major.minor.patch")
set(TWEAKER_PLUGIN_BUILD "0" CACHE STRING "TweakerPlugin build number, the fourth VERSIONINFO field")

string(REPLACE "." ";" _tweaker_version_parts "${TWEAKER_PLUGIN_VERSION}")
list(LENGTH _tweaker_version_parts _tweaker_version_count)
if(NOT _tweaker_version_count EQUAL 3)
    message(FATAL_ERROR "TWEAKER_PLUGIN_VERSION must be major.minor.patch, got '${TWEAKER_PLUGIN_VERSION}'")
endif()
list(GET _tweaker_version_parts 0 TWEAKER_PLUGIN_VERSION_MAJOR)
list(GET _tweaker_version_parts 1 TWEAKER_PLUGIN_VERSION_MINOR)
list(GET _tweaker_version_parts 2 TWEAKER_PLUGIN_VERSION_PATCH)

configure_file("${CMAKE_SOURCE_DIR}/src/version.rc.in" "${CMAKE_BINARY_DIR}/generated/version.rc" @ONLY)
target_sources(TweakerPlugin PRIVATE "${CMAKE_BINARY_DIR}/generated/version.rc")
target_compile_definitions(TweakerPlugin PRIVATE TW_PLUGIN_VERSION="${TWEAKER_PLUGIN_VERSION}.${TWEAKER_PLUGIN_BUILD}")

set_target_properties(TweakerPlugin PROPERTIES
    OUTPUT_NAME "TweakerPlugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Lua scripts ship as loose files next to the DLL rather than embedded in it the way fonts and icons
# are: the whole point of the scripting layer is that a user can open one in an editor, change it,
# and see the result without a rebuild. assets/scripts/ is the bundled starting set.
#
# Lives here rather than in src/lua/CMakeLists.txt because add_custom_command(TARGET) has to be in
# the directory that created the target.
# No POST_BUILD copy of assets/scripts any more. It existed because the plugin used to load its Lua
# files from a scripts/ folder beside the DLL, back when the DLL was injected out of the TweakerUI
# folder. The plugin now resolves every path from QuestViewer.exe instead (plugin/paths.cxx), so the
# scripts it reads live in engine\TweakerStuff\Scripts\ and nothing ever looked at a copy in the build
# tree: the harnesses read assets/scripts straight out of the source tree, the bundle is assembled by
# CopyTweakerPlugin in TweakerUI.csproj, and the game gets its copy from PluginInstallation.
#
# Relocating that copy to $<TARGET_FILE_DIR>/TweakerStuff/Scripts (as Ф6 originally proposed) would
# only move output nobody reads - and a second folder full of .lua files, sitting next to the DLL and
# silently stale, is exactly the "which of these is the real one" question worth not having.

# Each preset writes compile_commands.json under build/<preset>/, but clangd
# wants a single stable path at the project root. Refresh it on every build.
#
# Only Ninja/Makefiles generators actually write compile_commands.json - the automated
# dotnet-triggered build (TweakerUI.csproj's BuildTweakerPlugin target) forces the Visual Studio
# generator in its own separate build/msbuild tree (to get x86 output regardless of the calling
# shell's architecture, same reasoning as InjectHelper's build hook), and VS never produces that
# file. Gating on CMAKE_GENERATOR at configure time means the target simply doesn't exist there,
# rather than existing and failing on a copy_if_different against a file that will never appear.
if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
add_custom_target(copy_compile_commands ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    BYPRODUCTS "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Copying compile_commands.json (${CMAKE_BUILD_TYPE}) to project root"
    VERBATIM
)
endif()
