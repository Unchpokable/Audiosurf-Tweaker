#include "ui/widgets/slider.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>

namespace
{
constexpr float k_track_h = 4.f;
constexpr float k_thumb_r = 7.f;
constexpr float k_label_gap = 4.f;
constexpr float k_value_gap = 8.f;
constexpr float k_default_w = 200.f;

// Coarse drag maps the cursor straight onto the track; fine drag moves at a fraction of that.
// Needed rather than nice to have: the shared palette declares "Light size" over 0.99..1.0, which on
// a 200px track is a whole range inside one and a half pixels.
constexpr float k_fine_scale = 0.15f;

constexpr int k_hover_ms = 150;
constexpr int k_active_ms = 90;

// Decimals from the width of the range rather than a fixed count. The shared palette alone spans
// "Light size" at 0.99..1.0 and "Light glow" at 1..1000: two decimals make the first read 1.00
// across its whole travel, four make the second read 320.0000.
const char* format_for_span(float span) noexcept
{
    if(span >= 100.f) {
        return "%.0f";
    }
    if(span >= 10.f) {
        return "%.1f";
    }
    if(span >= 1.f) {
        return "%.2f";
    }
    return "%.4f";
}
} // namespace

namespace tw::ui::widgets
{
slider::slider(const char* id, ImVec2 size) : m_id(id ? id : "slider"), m_size(size)
{
}

void slider::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void slider::set_range(float min_value, float max_value) noexcept
{
    m_min = min_value;
    m_max = max_value > min_value ? max_value : min_value + 1.f;
    m_value = std::clamp(m_value, m_min, m_max);
}

void slider::set_label(std::string_view label)
{
    m_label.assign(label);
}

void slider::set_value(float value) noexcept
{
    // A gesture in progress owns the value. The panel pushes the authoritative number back every
    // frame, and during a drag that number came from this widget one frame ago - letting it land
    // again would fight the rounding rather than agree with it.
    if(m_editing || m_was_active) {
        return;
    }

    const float clamped = std::clamp(value, m_min, m_max);
    if(clamped == m_value) {
        return;
    }

    m_value = clamped;
}

void slider::begin_edit()
{
    m_editing = true;
    m_focus_pending = true;
    m_edit_value = m_value;
}

float slider::value_from_x(float mouse_x, float track_x0, float track_w) const noexcept
{
    const float t = (mouse_x - track_x0) / track_w;
    return m_min + t * (m_max - m_min);
}

void slider::update()
{
    m_changed = false;
    m_committed = false;

    ImGui::PushID(m_id.c_str());

    const float line_h = ImGui::GetFontSize();

    // The control lane is as tall as a line of text even though the thumb is shorter, so the row
    // keeps one height whether it is showing a track or the edit field that replaces it.
    const float control_h = (std::max)(k_thumb_r * 2.f, line_h);
    const float row_h = line_h + k_label_gap + control_h;

    const ImVec2 size = detail::resolve_size(m_size, k_default_w, row_h);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float span = m_max - m_min;
    const char* fmt = format_for_span(span);

    char value_text[32];
    std::snprintf(value_text, sizeof(value_text), fmt, static_cast<double>(m_editing ? m_edit_value : m_value));
    const ImVec2 value_size = ImGui::CalcTextSize(value_text);

    if(!m_label.empty()) {
        detail::add_text_ellipsis(
            draw, pos, bb.Max.x - value_size.x - k_value_gap, detail::to_u32(theme::text_secondary), m_label.c_str());
    }

    if(m_editing) {
        ImGui::SetCursorScreenPos(ImVec2 { bb.Min.x, bb.Min.y + line_h + k_label_gap });
        ImGui::SetNextItemWidth(size.x);

        if(m_focus_pending) {
            ImGui::SetKeyboardFocusHere();
            m_focus_pending = false;
        }

        // Zero vertical frame padding keeps the field exactly one line tall, which is what makes the
        // row the same height in both modes - anything else makes the panel jump on a double click.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2 { 6.f, 0.f });
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::surface_input);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::surface_hover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme::surface_elevated);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::PushStyleColor(ImGuiCol_Border, theme::accent_border);

        const bool entered = ImGui::InputFloat(
            "##edit", &m_edit_value, 0.f, 0.f, fmt, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);

        // Both queried immediately after the widget, before anything else can become "the item".
        const bool after_edit = ImGui::IsItemDeactivatedAfterEdit();
        const bool deactivated = ImGui::IsItemDeactivated();

        if(entered || after_edit) {
            const float next = std::clamp(m_edit_value, m_min, m_max);
            if(next != m_value) {
                m_value = next;
                m_changed = true;
            }
            m_committed = true;
            m_editing = false;
        }
        else if(deactivated) {
            // Escape, or a click elsewhere without typing. ImGui has already reverted its buffer.
            m_editing = false;
        }

        // The field advanced the cursor by its own height; claim the row's real extent instead, so
        // the next parameter lands where it would have without the edit.
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(size);
        ImGui::PopID();
        return;
    }

    ImGui::ItemSize(bb);
    const ImGuiID imgui_id = ImGui::GetID("##slider");
    if(!ImGui::ItemAdd(bb, imgui_id)) {
        ImGui::PopID();
        return;
    }

    bool hovered = false;
    bool held = false;
    ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);

    const float control_cy = bb.Min.y + line_h + k_label_gap + control_h * 0.5f;
    const float track_x0 = bb.Min.x + k_thumb_r;
    const float track_x1 = bb.Max.x - k_thumb_r;
    const float track_w = (std::max)(1.f, track_x1 - track_x0);

    // The whole row is the hit target, label included - a 4px track is not something anyone should
    // have to aim at.
    if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        begin_edit();
        held = false;
    }
    else if(held) {
        const bool shift = ImGui::GetIO().KeyShift;
        const ImVec2 mouse = ImGui::GetMousePos();

        // Re-anchor on gesture start and on every Shift transition - without the latter, switching
        // into fine mode teleports the thumb to where the coarse mapping would have put it.
        if(!m_was_active || shift != m_was_shift) {
            m_anchor_x = mouse.x;
            m_anchor_value = m_value;
            m_was_shift = shift;
        }

        float next = shift
            ? m_anchor_value + (mouse.x - m_anchor_x) / track_w * span * k_fine_scale
            : value_from_x(mouse.x, track_x0, track_w);
        next = std::clamp(next, m_min, m_max);

        if(next != m_value) {
            m_value = next;
            m_changed = true;
        }
    }

    if(ImGui::IsItemDeactivated()) {
        m_committed = true;
    }

    const auto dt = detail::dt_ms();
    if(hovered != m_was_hovered) {
        m_hover_tween = tweeny::from(m_hover_t).to(hovered ? 1.f : 0.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
    }
    if(held != m_was_active) {
        m_active_tween = tweeny::from(m_active_t).to(held ? 1.f : 0.f).during(k_active_ms).via(tweeny::easing::quadraticOut);
    }
    m_was_hovered = hovered;
    m_was_active = held;

    if(!m_hover_tween.isFinished()) {
        m_hover_t = m_hover_tween.step(dt);
    }
    if(!m_active_tween.isFinished()) {
        m_active_t = m_active_tween.step(dt);
    }

    const ImVec4 value_col = detail::lerp(theme::text_primary, theme::accent_text, m_active_t);
    draw->AddText(ImVec2 { bb.Max.x - value_size.x, bb.Min.y }, detail::to_u32(value_col), value_text);

    const float ty0 = control_cy - k_track_h * 0.5f;
    const float ty1 = ty0 + k_track_h;
    const float rounding = k_track_h * 0.5f;

    detail::add_rect_filled_rounded(
        draw, ImVec2 { track_x0, ty0 }, ImVec2 { track_x1, ty1 }, detail::to_u32(theme::control_track_off), rounding);

    const float t = span > 0.f ? (m_value - m_min) / span : 0.f;
    const float thumb_cx = track_x0 + t * track_w;

    if(thumb_cx > track_x0 + 0.5f) {
        detail::add_rect_filled_rounded(
            draw, ImVec2 { track_x0, ty0 }, ImVec2 { thumb_cx, ty1 }, detail::to_u32(theme::accent_primary), rounding);
    }

    const ImVec2 thumb_center { thumb_cx, control_cy };
    detail::add_circle_glow(draw, thumb_center, k_thumb_r, theme::accent_primary, (std::max)(m_hover_t, m_active_t));
    draw->AddCircleFilled(thumb_center, k_thumb_r - 1.f + m_active_t, detail::to_u32(theme::control_thumb));
    draw->AddCircle(thumb_center, k_thumb_r, detail::to_u32(theme::border_strong));

    ImGui::PopID();
}

float slider::value() const noexcept
{
    return m_value;
}

bool slider::changed() const noexcept
{
    return m_changed;
}

bool slider::committed() const noexcept
{
    return m_committed;
}
} // namespace tw::ui::widgets
