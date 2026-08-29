#include "ui/widgets/color_field.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <algorithm>

namespace
{
constexpr float k_label_gap = 8.f;
constexpr float k_picker_w = 260.f;
constexpr int k_hover_ms = 150;
} // namespace

namespace tw::ui::widgets
{
color_field::color_field(const char* id, float swatch_diameter)
    : m_id(id ? id : "color_field")
    , m_diameter(swatch_diameter)
    , m_popup((m_id + "_popup").c_str())
    , m_picker((m_id + "_picker").c_str(), ImVec2 { k_picker_w, 0.f })
{
}

void color_field::set_label(std::string_view label)
{
    m_label.assign(label);
}

void color_field::set_color(const ImVec4& rgba) noexcept
{
    // While the popup is up the picker owns the value: the caller is echoing back what this widget
    // told it one frame ago, and letting that land would fight the drag in progress.
    if(m_popup.opened()) {
        return;
    }

    m_color = rgba;
}

void color_field::update()
{
    m_changed = false;
    m_committed = false;

    ImGui::PushID(m_id.c_str());

    const float d = m_diameter > 0.f ? m_diameter : ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float row_w = (std::max)(d, ImGui::GetContentRegionAvail().x);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    if(!m_label.empty()) {
        const ImVec2 text_pos { pos.x, pos.y + (d - ImGui::GetFontSize()) * 0.5f };
        detail::add_text_ellipsis(
            draw, text_pos, pos.x + row_w - d - k_label_gap, detail::to_u32(theme::text_secondary), m_label.c_str());
    }

    // Swatch pinned to the right edge, the same way every label/control row in the overlay is laid
    // out (see the toggle rows in menu.cxx and the Skybox tab).
    ImGui::SetCursorScreenPos(ImVec2 { pos.x + row_w - d, pos.y });
    ImGui::InvisibleButton("##swatch", ImVec2 { d, d });

    const bool hovered = ImGui::IsItemHovered();
    if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_picker.set_color(m_color);
        m_popup.set_title(m_label);
        m_popup.open();
    }

    const auto dt = detail::dt_ms();
    if(hovered != m_was_hovered) {
        m_hover_tween = tweeny::from(m_hover_t).to(hovered ? 1.f : 0.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
    }
    m_was_hovered = hovered;
    if(!m_hover_tween.isFinished()) {
        m_hover_t = m_hover_tween.step(dt);
    }

    const ImVec2 center { pos.x + row_w - d * 0.5f, pos.y + d * 0.5f };

    // Glow takes the swatch's own hue so the hover reads as "this colour", not as a generic accent.
    const ImVec4 glow_tint { m_color.x, m_color.y, m_color.z, 1.f };
    detail::add_circle_glow(draw, center, d * 0.5f, glow_tint, m_hover_t);
    draw->AddCircleFilled(center, d * 0.5f - 1.5f, detail::to_u32(m_color));
    draw->AddCircle(center, d * 0.5f, detail::to_u32(theme::border));

    // The swatch positioned itself absolutely; claim the whole row so the next field lands under it.
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2 { row_w, d });

    if(m_popup.opened()) {
        m_popup.begin();
        m_picker.update(m_color);
        if(m_picker.changed()) {
            m_changed = true;
            m_dirty = true;
        }
        m_popup.end();
    }

    // One commit per gesture, on the frame the mouse comes up - the same lazy-write rule the menu
    // uses for its config flush.
    if(m_dirty && !ImGui::IsAnyMouseDown()) {
        m_dirty = false;
        m_committed = true;
    }

    ImGui::PopID();
}

ImVec4 color_field::color() const noexcept
{
    return m_color;
}

bool color_field::changed() const noexcept
{
    return m_changed;
}

bool color_field::committed() const noexcept
{
    return m_committed;
}
} // namespace tw::ui::widgets
