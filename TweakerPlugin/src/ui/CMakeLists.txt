# Shared UI widgets + theme (used by TweakerPlugin DLL and smoke_test).
add_library(tweaker_ui STATIC
    theme.cxx
    widgets/button.cxx
    widgets/toggle.cxx
    widgets/list_item.cxx
    widgets/list_view.cxx
)

target_include_directories(tweaker_ui PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(tweaker_ui PUBLIC imgui)

# Remaining overlay/plugin sources stay on the DLL target.
target_sources(TweakerPlugin PRIVATE
    ui_main.cxx
    overlay_state.cxx
)

target_link_libraries(TweakerPlugin PRIVATE tweaker_ui)
