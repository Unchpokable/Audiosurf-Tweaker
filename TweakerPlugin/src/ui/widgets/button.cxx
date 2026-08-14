#include "ui/widgets/button.hxx"

#include "ui/image/svg.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

namespace
{
constexpr float k_rounding = 8.f;
constexpr int k_hover_ms = 150;
constexpr int k_press_ms = 90;
constexpr float k_icon_gap = 6.f; // between icon and label; unused when the button is icon-only

// Icons are baked at 16/24/32 (see ui/image/svg.hxx's k_baked_sizes), and landing on one of those
// exactly is what keeps ImGui from resampling the texture. Picking by button height rather than
// always using 16 keeps a 40px transport button from looking like a mostly-empty square.
int icon_size_for(float button_height) noexcept
{
    return button_height >= 36.f ? 24 : 16;
}
} // namespace

namespace tw::ui::widgets
{
button::button(const char* id, ImVec2 size) : m_id(id ? id : "button"), m_size(size)
{
}

void button::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void button::set_label(std::string_view label)
{
    m_label.assign(label.begin(), label.end());
}

void button::set_icon(std::string_view resource_key)
{
    m_icon_key.assign(resource_key.begin(), resource_key.end());
}

void button::retarget_hover(float target)
{
    m_hover_tween = tweeny::from(m_hover_t).to(target).during(k_hover_ms).via(tweeny::easing::cubicOut);
}

void button::retarget_press(float target)
{
    m_press_tween = tweeny::from(m_press_t).to(target).during(k_press_ms).via(tweeny::easing::quadraticOut);
}

void button::update(const char* label)
{
    m_clicked = false;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = detail::resolve_size(m_size, 120.f, 32.f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    ImGui::ItemSize(bb);
    const ImGuiID imgui_id = ImGui::GetID("##btn");
    if(!ImGui::ItemAdd(bb, imgui_id)) {
        ImGui::PopID();
        return;
    }

    m_hovered = false;
    m_held = false;
    m_clicked = ImGui::ButtonBehavior(bb, imgui_id, &m_hovered, &m_held);

    if(m_hovered != m_was_hovered) {
        retarget_hover(m_hovered ? 1.f : 0.f);
    }
    if(m_held != m_was_held) {
        retarget_press(m_held ? 1.f : 0.f);
    }

    m_was_hovered = m_hovered;
    m_was_held = m_held;

    // Own floats are source of truth — tweeny::from(x) does not init peek()/current.
    const auto dt = detail::dt_ms();
    if(!m_hover_tween.isFinished()) {
        m_hover_t = m_hover_tween.step(dt);
    }
    if(!m_press_tween.isFinished()) {
        m_press_t = m_press_tween.step(dt);
    }

    const ImVec4 hover_col = detail::lerp(theme::surface_soft, theme::accent_primary, 0.35f * m_hover_t);
    const ImVec4 fill = detail::lerp(hover_col, theme::accent_pressed, m_press_t);
    const ImVec4 text_col = detail::lerp(theme::text_primary, theme::text_on_accent, m_press_t);

    const float scale = 1.f - 0.04f * m_press_t;
    const ImVec2 center { (bb.Min.x + bb.Max.x) * 0.5f, (bb.Min.y + bb.Max.y) * 0.5f };
    const ImVec2 half { (bb.Max.x - bb.Min.x) * 0.5f * scale, (bb.Max.y - bb.Min.y) * 0.5f * scale };
    const ImVec2 r_min { center.x - half.x, center.y - half.y };
    const ImVec2 r_max { center.x + half.x, center.y + half.y };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    detail::add_rect_filled_rounded(draw, r_min, r_max, detail::to_u32(fill), k_rounding);
    draw->AddRect(r_min, r_max, detail::to_u32(theme::accent_border), k_rounding);

    const char* text = label ? label : m_label.c_str();
    const bool has_text = text != nullptr && text[0] != '\0';

    // Resolved per frame, never cached: an ImTextureID goes stale when the render device is
    // replaced. A missing key resolves to ImTextureID_Invalid and simply contributes nothing.
    const float icon_px = static_cast<float>(icon_size_for(bb.Max.y - bb.Min.y));
    const ImTextureID icon = m_icon_key.empty() ? ImTextureID_Invalid : image::svg::get_resource(m_icon_key).at(static_cast<int>(icon_px));
    const bool has_icon = icon != ImTextureID_Invalid;

    // Icon and label are centred as one group, so an icon-only button centres its icon and a
    // labelled one keeps the pair visually balanced instead of pinning the icon to an edge.
    const ImVec2 text_size = has_text ? ImGui::CalcTextSize(text) : ImVec2 { 0.f, 0.f };
    const float group_w = (has_icon ? icon_px : 0.f) + (has_icon && has_text ? k_icon_gap : 0.f) + text_size.x;
    float cursor_x = center.x - group_w * 0.5f;

    if(has_icon) {
        const ImVec2 icon_min { cursor_x, center.y - icon_px * 0.5f };
        const ImVec2 icon_max { icon_min.x + icon_px, icon_min.y + icon_px };
        // Assets are monochrome white, so tinting with the label colour is what keeps the icon on
        // theme and in step with the press animation.
        detail::add_image_keep_aspect(draw, icon, ImVec2 { 0.f, 0.f }, icon_min, icon_max, detail::to_u32(text_col));
        cursor_x += icon_px + (has_text ? k_icon_gap : 0.f);
    }

    if(has_text) {
        draw->AddText(ImVec2 { cursor_x, center.y - text_size.y * 0.5f }, detail::to_u32(text_col), text);
    }

    ImGui::PopID();
}

bool button::clicked() const noexcept
{
    return m_clicked;
}

bool button::hovered() const noexcept
{
    return m_hovered;
}
} // namespace tw::ui::widgets
