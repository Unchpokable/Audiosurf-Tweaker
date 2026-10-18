#include "ui/widgets/segmented.hxx"

#include "ui/image/svg.hxx"
#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>

namespace
{
constexpr float k_chip_pad_x = 12.f; // text inset on each side of a chip
constexpr float k_chip_min_w = 56.f; // so a one-word chip ("Single") still reads as a button
constexpr float k_track_pad = tw::ui::theme::space::strip_pad;

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
        m_chips[static_cast<std::size_t>(m_selected)].select.snap(1.f);
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
    // Per chip, as update() draws it: a chip past the end of the icon list shows its label, and a label
    // in an icon-sized chip ran out of it.
    if(index < m_icon_keys.size()) {
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

void segmented::set_enabled(bool enabled) noexcept
{
    m_enabled = enabled;
}

float segmented::height() const noexcept
{
    return m_height;
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
    // its width is the sum of theirs. Kept on the chips, not in a vector allocated per frame (U2).
    float total_w = k_track_pad;
    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        m_chips[i].width = chip_width(i);
        total_w += m_chips[i].width + k_track_pad;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 track_min = origin;
    const ImVec2 track_max { origin.x + total_w, origin.y + m_height };

    ImGui::ItemSize(ImRect { track_min, track_max });
    if(!ImGui::ItemAdd(ImRect { track_min, track_max }, ImGui::GetID("##track"))) {
        ImGui::PopID();
        return;
    }

    const bool fade = kit::fade_disabled(m_enabled);
    if(fade) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * theme::disabled_alpha);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(track_min, track_max, kit::paint::to_u32(theme::surface_muted), m_rounding);
    draw->AddRect(track_min, track_max, kit::paint::to_u32(theme::border_subtle), m_rounding);

    const float chip_h = m_height - k_track_pad * 2.f;
    float x = track_min.x + k_track_pad;

    // Concentric with the track's corners (theme::radius, review R17): theme::radius::chip at the default.
    const float chip_rounding = (std::max)(0.f, m_rounding - k_track_pad);

    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        chip_state& chip = m_chips[i];

        const ImVec2 chip_min { x, track_min.y + k_track_pad };
        const ImVec2 chip_max { x + chip.width, chip_min.y + chip_h };
        const ImRect bb { chip_min, chip_max };

        // Overlay: the strip claimed its whole width as one item above, the chips only hit-test inside it.
        ImGui::PushID(static_cast<int>(i));
        const kit::interaction pointer = kit::interact(bb, ImGui::GetID("##chip"), kit::placement::overlay, m_enabled);
        ImGui::PopID();

        const bool hovered = pointer.hovered;
        if(hovered) {
            m_hovered_index = static_cast<int>(i);
        }

        if(pointer.clicked && m_selected != static_cast<int>(i)) {
            m_selected = static_cast<int>(i);
            m_changed = true;
        }

        chip.hover.set(hovered);
        chip.select.set(m_selected == static_cast<int>(i));
        const float hover_t = chip.hover.step();
        const float select_t = chip.select.step();

        // Hover reads on the unselected chips only; on the selected one it would fight the accent
        // fill and make the strip look like it had two selections during the crossfade.
        const ImVec4 idle = kit::paint::lerp(theme::surface_muted, theme::surface_hover, hover_t * (1.f - select_t));
        const ImVec4 fill = kit::paint::lerp(idle, theme::accent_soft, select_t);
        const ImVec4 text_col = kit::paint::lerp(theme::text_secondary, theme::accent_text, select_t);

        draw->AddRectFilled(chip_min, chip_max, kit::paint::to_u32(fill), chip_rounding);
        if(select_t > 0.01f) {
            draw->AddRect(chip_min,
                chip_max,
                kit::paint::to_u32(
                    ImVec4 { theme::accent_border.x, theme::accent_border.y, theme::accent_border.z, theme::accent_border.w * select_t }),
                chip_rounding);
        }

        if(i < m_icon_keys.size()) {
            // Resolved per frame, never cached - an ImTextureID only survives until the render
            // device is replaced. Monochrome assets, so the label colour tints them and the glyph
            // follows the selection crossfade for free.
            const ImTextureID icon = tw::ui::image::svg::get_resource(m_icon_keys[i]).sz16;
            const ImVec2 icon_min { chip_min.x + (chip.width - k_icon_px) * 0.5f, chip_min.y + (chip_h - k_icon_px) * 0.5f };
            kit::paint::icon(draw, icon, icon_min, k_icon_px, text_col);
        }
        else {
            const ImVec2 text_size = ImGui::CalcTextSize(m_labels[i].c_str());
            const ImVec2 text_pos {
                chip_min.x + (chip.width - text_size.x) * 0.5f,
                chip_min.y + (chip_h - text_size.y) * 0.5f,
            };
            kit::paint::add_text_glow(draw, text_pos, m_labels[i].c_str(), kit::paint::to_u32(text_col), theme::accent_primary, select_t);
        }

        x += chip.width + k_track_pad;
    }

    if(fade) {
        ImGui::PopStyleVar();
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
