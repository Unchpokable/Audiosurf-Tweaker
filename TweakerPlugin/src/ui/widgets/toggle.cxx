#include "ui/widgets/toggle.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <imgui_internal.h>

namespace tw::ui::widgets
{
toggle::toggle(const char* id, ImVec2 size) : m_id(id ? id : "toggle"), m_size(size)
{
}

void toggle::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

ImVec2 toggle::size() const noexcept
{
    return m_size;
}

void toggle::set_checked(bool checked)
{
    if(!m_drawn) {
        snap(checked);
        return;
    }
    if(m_checked == checked) {
        return;
    }
    m_checked = checked;
    m_thumb.set(m_checked);
}

void toggle::snap(bool checked) noexcept
{
    m_checked = checked;
    m_thumb.snap(checked ? 1.f : 0.f);
}

void toggle::set_enabled(bool enabled) noexcept
{
    m_enabled = enabled;
}

void toggle::update()
{
    m_changed = false;
    m_drawn = true;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = kit::paint::resolve_size(m_size, 40.f, 22.f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    const kit::interaction pointer = kit::interact(bb, ImGui::GetID("##toggle"), kit::placement::flow, m_enabled);
    m_hovered = pointer.hovered;
    m_held = pointer.held;
    if(!pointer.visible) {
        ImGui::PopID();
        return;
    }

    if(pointer.clicked) {
        m_checked = !m_checked;
        m_changed = true;
        m_thumb.set(m_checked);
    }

    m_hover.set(m_hovered);
    m_press.set(m_held);
    const float thumb_t = m_thumb.step();
    const float hover_t = m_hover.step();
    const float press_t = m_press.step();

    const bool fade = kit::fade_disabled(m_enabled);
    if(fade) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * theme::disabled_alpha);
    }

    const float rounding = size.y * 0.5f;
    const ImVec4 track = kit::paint::lerp(theme::control_track_off, theme::accent_primary, thumb_t);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(bb.Min, bb.Max, kit::paint::to_u32(track), rounding);

    // Pressed, the thumb stretches toward the side it would travel to (R13) - grows from its own end, so
    // a thumb resting at the right grows leftwards and the other way round.
    constexpr float k_stretch = 5.f;
    const float pad = 2.f;
    const float thumb_d = size.y - pad * 2.f;
    const float stretch = k_stretch * press_t;
    const float travel = size.x - pad * 2.f - thumb_d;
    const float thumb_x = bb.Min.x + pad + travel * thumb_t - stretch * thumb_t;
    const float thumb_y = bb.Min.y + pad;
    const ImVec2 t_min { thumb_x, thumb_y };
    const ImVec2 t_max { thumb_x + thumb_d + stretch, thumb_y + thumb_d };

    // Hovered, the thumb glows the way the slider's does (R13).
    kit::paint::add_circle_glow(
        draw, ImVec2 { (t_min.x + t_max.x) * 0.5f, (t_min.y + t_max.y) * 0.5f }, thumb_d * 0.5f, theme::accent_primary, hover_t);
    draw->AddRectFilled(t_min, t_max, kit::paint::to_u32(theme::control_thumb), thumb_d * 0.5f);

    if(fade) {
        ImGui::PopStyleVar();
    }

    ImGui::PopID();
}

bool toggle::checked() const noexcept
{
    return m_checked;
}

bool toggle::changed() const noexcept
{
    return m_changed;
}

bool toggle::hovered() const noexcept
{
    return m_hovered;
}
} // namespace tw::ui::widgets
