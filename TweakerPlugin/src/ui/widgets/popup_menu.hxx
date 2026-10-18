#pragma once

#include <imgui.h>

#include <string>
#include <string_view>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
class popup_menu {
public:
    explicit popup_menu(const char* id, const char* title = "");

    void set_title(std::string_view title);
    void set_rounding(float rounding) noexcept;
    void set_inner_padding(ImVec2 padding) noexcept;

    // Captures GetMousePos() as the top-left corner and starts fade-in.
    void open();

    // Opens under the rect of whatever opened it - a button, a row - left edges aligned, or above it when
    // there is no room below. For a popup that belongs to a control, where the pointer's position is an
    // accident of where on the control it was clicked.
    void open_at(ImVec2 anchor_min, ImVec2 anchor_max);
    // Starts fade-out; input is disabled immediately (NoInputs).
    void close();

    // True from open() until fade-out reaches zero (covers closing crossfade).
    [[nodiscard]] bool opened() const noexcept;

    void begin();
    void end();

private:
    void start_open();
    void retarget_opacity(float target);
    void step_opacity();
    void poll_dismiss();
    void clamp_origin_to_viewport(ImVec2 frame_size) noexcept;
    void paint_chrome(ImVec2 frame_min, ImVec2 frame_max) const;

    std::string m_id;
    std::string m_window_name;
    std::string m_title;
    float m_rounding = theme::radius::window;
    ImVec2 m_inner_padding { 8.f, 8.f };

    bool m_want_open = false;
    bool m_interactive = false;
    bool m_had_focus = false;
    int m_skip_dismiss_frames = 0;
    // Frames to force the popup above other windows after open(). A reappearing ImGui window does
    // not come to front on its own (NoFocusOnAppearing), so a reopen would otherwise render behind
    // whatever window was clicked to trigger it.
    int m_bring_to_front_frames = 0;

    kit::anim::channel m_opacity { theme::motion::transition };

    ImVec2 m_origin {};
    bool m_anchored = false;
    ImVec2 m_anchor_min {};
    ImVec2 m_anchor_max {};

    // Content goes into channel 1 and the chrome, drawn at end() once this frame's size is known, into
    // channel 0 under it (R14).
    ImDrawListSplitter m_splitter;
    bool m_split = false;
    ImVec2 m_frame_size {};
    ImVec2 m_frame_min {};
    ImVec2 m_frame_max {};
    float m_title_h = 0.f;

    bool m_active = false;
    bool m_skipped = false;
    bool m_window_begun = false;
    bool m_alpha_pushed = false;
};
} // namespace tw::ui::widgets
