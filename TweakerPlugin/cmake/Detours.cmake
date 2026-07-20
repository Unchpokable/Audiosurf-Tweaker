# Builds Microsoft Detours from the Detours/ git submodule
# (https://github.com/microsoft/Detours). If Detours/src is empty, fetch it
# with:
#   git submodule update --init --recursive
#
# Detours ships only an nmake Makefile upstream, no CMake support, so we
# mirror the source list from Detours/src/Makefile (the OBJS variable)
# ourselves instead of patching the submodule.

set(DETOURS_SRC_DIR "${CMAKE_SOURCE_DIR}/Detours/src")

if(NOT EXISTS "${DETOURS_SRC_DIR}/detours.cpp")
    message(FATAL_ERROR
        "Detours submodule not found at \"${DETOURS_SRC_DIR}\".\n"
        "Fetch it with:\n"
        "  git submodule update --init --recursive")
endif()

add_library(detours STATIC
    "${DETOURS_SRC_DIR}/detours.cpp"
    "${DETOURS_SRC_DIR}/modules.cpp"
    "${DETOURS_SRC_DIR}/disasm.cpp"
    "${DETOURS_SRC_DIR}/image.cpp"
    "${DETOURS_SRC_DIR}/creatwth.cpp"
    "${DETOURS_SRC_DIR}/disolx86.cpp"
    "${DETOURS_SRC_DIR}/disolx64.cpp"
    "${DETOURS_SRC_DIR}/disolia64.cpp"
    "${DETOURS_SRC_DIR}/disolarm.cpp"
    "${DETOURS_SRC_DIR}/disolarm64.cpp"
)

target_include_directories(detours PUBLIC "${DETOURS_SRC_DIR}")
target_compile_definitions(detours PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x501)
