target_sources(TweakerPlugin PRIVATE
    channel_hook.cxx
    d3d9_hooks.cxx
    detour_transaction.cxx
    dinput8_hooks.cxx
    imgui_backend.cxx
    texture_hook.cxx
    wndproc_hub.cxx
)

# DX9/Win32 ImGui backends - only the DLL needs these (smoke uses its own Win32/OpenGL3 backend
# pair, see smoke/CMakeLists.txt). Third-party sources: no TweakerPlugin PCH, /W3 instead of the
# project's default warning level.
target_sources(TweakerPlugin PRIVATE
    "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
)

target_include_directories(TweakerPlugin PRIVATE "${IMGUI_DIR}/backends")

set_source_files_properties(
    "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# dxguid supplies the storage for the IID_/GUID_ constants (IID_IDirectInput8A/W, GUID_SysMouse,
# ...) that dinput.h only declares extern - dinput8.lib itself is functions-only.
target_link_libraries(TweakerPlugin PRIVATE dinput8 dxguid)

if(MSVC)
    set_source_files_properties(
        "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        PROPERTIES COMPILE_OPTIONS "/W3"
    )
endif()
