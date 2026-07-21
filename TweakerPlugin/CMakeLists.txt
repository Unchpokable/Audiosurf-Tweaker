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

add_library(TweakerPlugin SHARED)
add_subdirectory(src)
add_subdirectory(smoke)

target_include_directories(TweakerPlugin PRIVATE src)

target_link_libraries(TweakerPlugin PRIVATE
    detours
    dxsdk
    quest3d_sdk
    imgui
)

target_precompile_headers(TweakerPlugin PRIVATE src/pch.hxx)

target_link_options(TweakerPlugin PRIVATE /STACK:4194304)

set_target_properties(TweakerPlugin PROPERTIES
    OUTPUT_NAME "TweakerPlugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

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
