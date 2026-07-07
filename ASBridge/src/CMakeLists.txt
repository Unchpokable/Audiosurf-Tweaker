add_executable(asbridge main.cxx)

target_compile_features(asbridge PUBLIC cxx_std_20)

target_include_directories(asbridge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(pipes)
