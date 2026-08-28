#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <string>
#include <string_view>

namespace tw::ui::widgets
{
// A float slider: label and value on one line, a thin track with a round thumb under it.
//
// Exists because there was no slider here at all, and the one place that needed one - the Skybox
// tab's shader parameters - reached for stock ImGui::SliderFloat and became the only raw-ImGui
// control in the overlay. ImGui's slider is a filled block with the number printed inside it; every
// other control here is a track with a thumb, so it read as a control borrowed from another program.
//
// The important part of the API is the split between changed() and committed(). A drag is hundreds
// of values and exactly one of them is worth keeping: the skybox pushes every intermediate value
// into the live shader so the sky moves under the mouse, and writes to its settings file only on
// release. Squashing both into one flag would either cost a full config rewrite per frame of drag or
// lose the live preview - see skybox::set_program_param's `persist` argument.
class slider {
public:
    explicit slider(const char* id, ImVec2 size = { 0.f, 0.f });

    void set_size(ImVec2 size) noexcept;

    // Clamps the current value into the new range, including when the range shrinks under it - a
    // widget must never show a number it cannot produce (same contract as number_input::set_range).
    void set_range(float min_value, float max_value) noexcept;

    void set_label(std::string_view label);

    // Programmatic set - no-op if unchanged, never raises changed()/committed(), and ignored while
    // the user is dragging or typing, so an owner pushing the authoritative value every frame cannot
    // fight the gesture in progress.
    void set_value(float value) noexcept;

    void update();

    [[nodiscard]] float value() const noexcept;

    // True on every frame the value moved - that is the point, this is the live-preview signal.
    [[nodiscard]] bool changed() const noexcept;

    // True on the single frame a gesture ended: mouse released after a drag, or a typed value
    // accepted. This is the one to persist on.
    [[nodiscard]] bool committed() const noexcept;

private:
    void begin_edit();
    [[nodiscard]] float value_from_x(float mouse_x, float track_x0, float track_w) const noexcept;

    std::string m_id;
    std::string m_label;
    ImVec2 m_size;

    float m_value = 0.f;
    float m_min = 0.f;
    float m_max = 1.f;

    float m_hover_t = 0.f;
    float m_active_t = 0.f;
    tweeny::tween<float> m_hover_tween {};
    tweeny::tween<float> m_active_tween {};
    bool m_was_hovered = false;
    bool m_was_active = false;

    // Drag is re-anchored whenever Shift is pressed or released mid-gesture. Without that, switching
    // into fine mode teleports the thumb to wherever the coarse mapping would have put the cursor.
    float m_anchor_x = 0.f;
    float m_anchor_value = 0.f;
    bool m_was_shift = false;

    bool m_editing = false;
    bool m_focus_pending = false;
    float m_edit_value = 0.f;

    bool m_changed = false;
    bool m_committed = false;
};
} // namespace tw::ui::widgets
