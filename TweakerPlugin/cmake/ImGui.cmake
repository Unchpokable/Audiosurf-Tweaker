# Vendored Dear ImGui under ImGui/ (typically gitignored; clone/copy locally to build).
# Core sources only — DX9/Win32 backends are wired separately when the overlay bind lands.

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/ImGui")

if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR
        "Dear ImGui not found at \"${IMGUI_DIR}\".\n"
        "Place a Dear ImGui tree there (imgui.h + imgui*.cpp).")
endif()

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)

target_include_directories(imgui PUBLIC "${IMGUI_DIR}")

# ImGui is third-party; keep it free of TweakerPlugin's PCH / warning level.
if(MSVC)
    target_compile_options(imgui PRIVATE /W3)
endif()
