#include "ui/widgets/segmented.hxx"

#include "ui/image/svg.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>

namespace
{
constexpr float k_chip_pad_x = 12.f; // text inset on each side of a chip
constexpr float k_chip_min_w = 56.f; // so a one-word chip ("Single") still reads as a button
constexpr float k_track_pad = 3.f;   // gap between the enclosing track and the chips inside it
constexpr int k_hover_ms = 150;
constexpr int k_select_ms = 150;

// Icon chips are square-ish rather than text-width: 16px glyph (a baked size, so ImGui does not
// resample it) plus symmetric padding, matching the desktop's own 16px mode glyphs.
constexpr float k_icon_px = 16.f;
constexpr float k_icon_chip_w = 34.f;
} // namespace

namespace tw::ui::widgets
{
segmented::segmented(const char* id, float height) : m_id(id ? id : "segmented"), m_height(height)
{
}

void segmented::set_height(float height) noexcept
{
    m_height = height;
}

void segmented::set_rounding(float rounding) noexcept
{
    m_rounding = rounding;
}

void segmented::set_options(std::span<const std::string_view> labels)
{
    m_labels.assign(labels.begin(), labels.end());
    m_chips.assign(m_labels.size(), chip_state {});

    if(m_selected >= static_cast<int>(m_labels.size())) {
        m_selected = 0;
    }

    // Seeded so the current selection is drawn selected on its very first frame rather than
    // animating in from nothing every time the options are rebuilt.
    if(m_selected >= 0 && m_selected < static_cast<int>(m_chips.size())) {
        m_chips[static_cast<std::size_t>(m_selected)].select_t = 1.f;
        m_chips[static_cast<std::size_t>(m_selected)].was_selected = true;
    }
}

void segmented::set_icons(std::span<const std::string_view> resource_keys)
{
    m_icon_keys.assign(resource_keys.begin(), resource_keys.end());
}

void segmented::set_selected(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(m_labels.size()) || index == m_selected) {
        return;
    }

    m_selected = index;
}

float segmented::chip_width(std::size_t index) const
{
    if(!m_icon_keys.empty()) {
        return k_icon_chip_w;
    }

    return (std::max)(k_chip_min_w, ImGui::CalcTextSize(m_labels[index].c_str()).x + k_chip_pad_x * 2.f);
}

float segmented::width() const
{
    if(m_labels.empty()) {
        return 0.f;
    }

    float total = k_track_pad;
    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        total += chip_width(i) + k_track_pad;
    }

    return total;
}

void segmented::update()
{
    m_changed = false;
    m_hovered_index = -1;

    if(m_labels.empty()) {
        return;
    }

    ImGui::PushID(m_id.c_str());

    // Measured first, drawn second: the enclosing track has to be laid down under the chips, and
    // its width is the sum of theirs.
    std::vector<float> widths(m_labels.size(), 0.f);
    float total_w = k_track_pad;
    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        widths[i] = chip_width(i);
        total_w += widths[i] + k_track_pad;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 track_min = origin;
    const ImVec2 track_max { origin.x + total_w, origin.y + m_height };

    ImGui::ItemSize(ImRect { track_min, track_max });
    if(!ImGui::ItemAdd(ImRect { track_min, track_max }, ImGui::GetID("##track"))) {
        ImGui::PopID();
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(track_min, track_max, detail::to_u32(theme::surface_muted), m_rounding);
    draw->AddRect(track_min, track_max, detail::to_u32(theme::border_subtle), m_rounding);

    const auto dt = detail::dt_ms();
    const float chip_h = m_height - k_track_pad * 2.f;
    float x = track_min.x + k_track_pad;

    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        chip_state& chip = m_chips[i];

        const ImVec2 chip_min { x, track_min.y + k_track_pad };
        const ImVec2 chip_max { x + widths[i], chip_min.y + chip_h };
        const ImRect bb { chip_min, chip_max };

        ImGui::PushID(static_cast<int>(i));
        const ImGuiID chip_id = ImGui::GetID("##chip");
        ImGui::ItemAdd(bb, chip_id);

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, chip_id, &hovered, &held);
        ImGui::PopID();

        if(hovered) {
            m_hovered_index = static_cast<int>(i);
        }

        if(pressed && m_selected != static_cast<int>(i)) {
            m_selected = static_cast<int>(i);
            m_changed = true;
        }

        const bool is_selected = m_selected == static_cast<int>(i);
        if(hovered != chip.was_hovered) {
            chip.hover_tween = tweeny::from(chip.hover_t).to(hovered ? 1.f : 0.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
            chip.was_hovered = hovered;
        }
        if(is_selected != chip.was_selected) {
            chip.select_tween = tweeny::from(chip.select_t).to(is_selected ? 1.f : 0.f).during(k_select_ms).via(tweeny::easing::cubicOut);
            chip.was_selected = is_selected;
        }

        // Own floats are the source of truth - tweeny::from(x) does not initialise peek()/current.
        if(!chip.hover_tween.isFinished()) {
            chip.hover_t = chip.hover_tween.step(dt);
        }
        if(!chip.select_tween.isFinished()) {
            chip.select_t = chip.select_tween.step(dt);
        }

        // Hover reads on the unselected chips only; on the selected one it would fight the accent
        // fill and make the strip look like it had two selections during the crossfade.
        const ImVec4 idle = detail::lerp(theme::surface_muted, theme::surface_hover, chip.hover_t * (1.f - chip.select_t));
        const ImVec4 fill = detail::lerp(idle, theme::accent_soft, chip.select_t);
        const ImVec4 text_col = detail::lerp(theme::text_secondary, theme::accent_text, chip.select_t);

        draw->AddRectFilled(chip_min, chip_max, detail::to_u32(fill), m_rounding - 1.f);
        if(chip.select_t > 0.01f) {
            draw->AddRect(chip_min,
                chip_max,
                detail::to_u32(ImVec4 { theme::accent_border.x, theme::accent_border.y, theme::accent_border.z,
                    theme::accent_border.w * chip.select_t }),
                m_rounding - 1.f);
        }

        if(i < m_icon_keys.size()) {
            // Resolved per frame, never cached - an ImTextureID only survives until the render
            // device is replaced. Monochrome assets, so the label colour tints them and the glyph
            // follows the selection crossfade for free.
            const ImTextureID icon = tw::ui::image::svg::get_resource(m_icon_keys[i]).sz16;
            const ImVec2 icon_min { chip_min.x + (widths[i] - k_icon_px) * 0.5f, chip_min.y + (chip_h - k_icon_px) * 0.5f };
            const ImVec2 icon_max { icon_min.x + k_icon_px, icon_min.y + k_icon_px };
            detail::add_image_keep_aspect(draw, icon, ImVec2 { 0.f, 0.f }, icon_min, icon_max, detail::to_u32(text_col));
        }
        else {
            const ImVec2 text_size = ImGui::CalcTextSize(m_labels[i].c_str());
            const ImVec2 text_pos {
                chip_min.x + (widths[i] - text_size.x) * 0.5f,
                chip_min.y + (chip_h - text_size.y) * 0.5f,
            };
            detail::add_text_glow(draw, text_pos, m_labels[i].c_str(), detail::to_u32(text_col), theme::accent_primary, chip.select_t);
        }

        x += widths[i] + k_track_pad;
    }

    ImGui::PopID();
}

int segmented::selected() const noexcept
{
    return m_selected;
}

bool segmented::changed() const noexcept
{
    return m_changed;
}

int segmented::hovered_index() const noexcept
{
    return m_hovered_index;
}
} // namespace tw::ui::widgets
