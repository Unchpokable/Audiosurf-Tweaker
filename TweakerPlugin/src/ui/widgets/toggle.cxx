#include "ui/widgets/toggle.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

namespace
{
constexpr int k_slide_ms = 200;
} // namespace

namespace tw::ui::widgets
{
toggle::toggle(const char* id, ImVec2 size) : m_id(id ? id : "toggle"), m_size(size)
{
}

void toggle::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void toggle::retarget(float target)
{
    m_thumb_tween = tweeny::from(m_thumb_t).to(target).during(k_slide_ms).via(tweeny::easing::cubicOut);
}

void toggle::set_checked(bool checked) noexcept
{
    if(m_checked == checked) {
        return;
    }
    m_checked = checked;
    retarget(m_checked ? 1.f : 0.f);
}

void toggle::update()
{
    m_changed = false;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = detail::resolve_size(m_size, 40.f, 22.f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    ImGui::ItemSize(bb);
    const ImGuiID imgui_id = ImGui::GetID("##toggle");
    if(!ImGui::ItemAdd(bb, imgui_id)) {
        ImGui::PopID();
        return;
    }

    m_hovered = false;
    m_held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, imgui_id, &m_hovered, &m_held);
    if(pressed) {
        m_checked = !m_checked;
        m_changed = true;
        retarget(m_checked ? 1.f : 0.f);
    }

    const auto dt = detail::dt_ms();
    if(!m_thumb_tween.isFinished()) {
        m_thumb_t = m_thumb_tween.step(dt);
    }

    const float rounding = size.y * 0.5f;
    const ImVec4 track = detail::lerp(theme::control_track_off, theme::accent_primary, m_thumb_t);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    detail::add_rect_filled_rounded(draw, bb.Min, bb.Max, detail::to_u32(track), rounding);

    const float pad = 2.f;
    const float thumb_d = size.y - pad * 2.f;
    const float travel = size.x - pad * 2.f - thumb_d;
    const float thumb_x = bb.Min.x + pad + travel * m_thumb_t;
    const float thumb_y = bb.Min.y + pad;
    const ImVec2 t_min { thumb_x, thumb_y };
    const ImVec2 t_max { thumb_x + thumb_d, thumb_y + thumb_d };
    detail::add_rect_filled_rounded(draw, t_min, t_max, detail::to_u32(theme::control_thumb), thumb_d * 0.5f);

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
