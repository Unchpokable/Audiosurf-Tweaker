# Shared UI widgets + theme + overlay data model (used by TweakerPlugin DLL and smoke_test).
# overlay_state has no D3D9/game dependency (pure data, see overlay_state.hxx) and lives here so
# smoke_test can seed it directly, bypassing real TW_OVL IPC, to exercise the real plugin UI
# modules with plausible data (see Docs/Internal/tweaker-plugin-widgets.md).
add_library(tweaker_ui STATIC
    theme.cxx
    overlay_state.cxx
    overlay_config.cxx
    texture_cache.cxx
    widgets/button.cxx
    widgets/toggle.cxx
    widgets/list_item.cxx
    widgets/list_view.cxx
    widgets/tab_view.cxx
    widgets/item_group.cxx
    widgets/popup_menu.cxx
    widgets/color_picker.cxx
    plugins/static/notefeed.cxx
    plugins/static/pins.cxx
    plugins/static/watermark.cxx
    plugins/interactive/menu.cxx
)

target_include_directories(tweaker_ui PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(tweaker_ui PUBLIC imgui)

# texture_cache.cxx decodes via stb_image and reads packed assets via tw::resource - both PRIVATE,
# tweaker_ui's own public headers never expose either.
target_link_libraries(tweaker_ui PRIVATE libstb tweaker_resource)

# Remaining overlay/plugin sources stay on the DLL target.
target_sources(TweakerPlugin PRIVATE
    ui_main.cxx
)

target_link_libraries(TweakerPlugin PRIVATE tweaker_ui)
