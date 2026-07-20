# Visual smoke harness for tw::ui::widgets (Win32 + OpenGL3).
# Not part of the default ALL build — request explicitly:
#   cmake --build --preset x86-release --target smoke_test
# or:
#   cmake --build --preset smoke

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/ImGui")

add_executable(smoke_test EXCLUDE_FROM_ALL
    main.cxx
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(smoke_test PRIVATE
    "${IMGUI_DIR}/backends"
)

target_link_libraries(smoke_test PRIVATE
    tweaker_ui
    opengl32
    gdi32
)

set_target_properties(smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "tweaker_ui_smoke"
)

if(MSVC)
    target_compile_options(smoke_test PRIVATE /W3)
    # Backends are third-party; skip TweakerPlugin PCH if inherited somehow.
    set_source_files_properties(
        "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endif()
