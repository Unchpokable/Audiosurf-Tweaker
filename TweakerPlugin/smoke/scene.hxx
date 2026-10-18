#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_state.hxx"

// One scene of the snapshot stand (shots.hxx): a fixed-size canvas, a function called once per frame,
// and the frames it asks to be captured. Everything a scene sees is deterministic - a fixed frame time,
// input it queues itself, the same fake host data every run - so two runs produce identical pixels and
// any difference between two shot directories is a difference in the code that drew them.
namespace tw::smoke
{
class scene_ctx {
public:
    scene_ctx(ImVec2 canvas, const ui::overlay_state::cache& overlay, const ui::qp::state::cache& quick_player) noexcept;

    [[nodiscard]] int frame() const noexcept;
    [[nodiscard]] ImVec2 canvas() const noexcept;
    [[nodiscard]] const ui::overlay_state::cache& overlay() const noexcept;
    [[nodiscard]] const ui::qp::state::cache& quick_player() const noexcept;

    // Input goes into ImGui's event queue, so it takes effect on the next frame - which is exactly when
    // a real event arriving between two frames would.
    void mouse_move(ImVec2 pos);
    void mouse_move(std::string_view tag, ImVec2 rel = { 0.5f, 0.5f });
    void mouse_button(ImGuiMouseButton button, bool down);
    void key(ImGuiKey key, bool down);
    void mouse_wheel(float steps); // positive scrolls up, as a wheel turned away from the user

    // Rects of things drawn on this or an earlier frame, under names the scene picks. A scene draws its
    // own widgets, so it can mark them; a rect never marked is empty.
    void mark(std::string_view tag);
    void mark(std::string_view tag, const ImRect& rect);
    [[nodiscard]] ImRect rect(std::string_view tag) const;
    [[nodiscard]] ImVec2 point(std::string_view tag, ImVec2 rel = { 0.5f, 0.5f }) const;

    // An ImGui window currently laid out, found by a substring of its name. For modules (menu, player)
    // whose items the stand cannot mark: their windows and child windows are the landmarks instead.
    // Empty when no live window matches.
    [[nodiscard]] ImRect window(std::string_view name_part) const;

    // Saves this frame, after it is rendered, as <scene>-<shot>.png.
    void capture(std::string_view shot);

    // This frame is the last one.
    void finish() noexcept;

    // Runner side.
    void begin_frame(int frame) noexcept;
    [[nodiscard]] std::vector<std::string> take_captures();
    [[nodiscard]] bool finished() const noexcept;

private:
    ImVec2 m_canvas;
    const ui::overlay_state::cache* m_overlay;
    const ui::qp::state::cache* m_quick_player;

    int m_frame = 0;
    bool m_finished = false;
    std::vector<std::pair<std::string, ImRect>> m_marks;
    std::vector<std::string> m_captures;
};

struct scene {
    const char* name;
    const char* description;
    ImVec2 canvas;

    // Once, before the first frame, with the ImGui context and all overlay modules initialized. May be
    // null.
    void (*setup)(scene_ctx& ctx);

    // Every frame, between NewFrame and Render.
    void (*frame)(scene_ctx& ctx);
};

std::span<const scene> all_scenes();

// The overlay as the DLL draws it each frame (ui_main.cxx): watermark, pins, notefeed, menu.
void draw_overlay(scene_ctx& ctx);
} // namespace tw::smoke
