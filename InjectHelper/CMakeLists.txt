cmake_minimum_required(VERSION 3.21)

project(InjectHelper LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "InjectHelper is Windows-only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "InjectHelper targets x86 only: it injects TweakerPlugin.dll into "
        "Audiosurf, a 32-bit process - the LoadLibraryA address it resolves "
        "via GetProcAddress(GetModuleHandle(\"kernel32.dll\")) is only valid "
        "for a same-bitness remote thread. Configure with a 32-bit "
        "generator/toolset (see TweakerUI.csproj's BuildInjectHelper target, "
        "which forces this with the Visual Studio generator's -A Win32).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(InjectHelper main.cpp)

set_target_properties(InjectHelper PROPERTIES
    OUTPUT_NAME "InjectHelper"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_compile_definitions(InjectHelper PRIVATE UNICODE _UNICODE)
