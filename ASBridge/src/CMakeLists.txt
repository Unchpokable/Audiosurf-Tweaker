add_executable(asbridge main.cxx)

target_include_directories(asbridge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(pipes)
add_subdirectory(window)
add_subdirectory(proto)
