#include "ui/widgets/color_field.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <algorithm>

namespace
{
constexpr float k_label_gap = 8.f;
constexpr float k_picker_w = 260.f;
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

void color_field::set_swatch_first(bool swatch_first) noexcept
{
    m_swatch_first = swatch_first;
}

void color_field::set_color(const ImVec4& rgba) noexcept
{
    // While the popup is up the picker owns the value: the caller is echoing back what this widget
    // told it one frame ago, and letting that land would fight the drag in progress.
    if(active()) {
        return;
    }

    m_color = rgba;
}

bool color_field::active() const noexcept
{
    return m_popup.opened();
}

void color_field::set_enabled(bool enabled) noexcept
{
    m_enabled = enabled;
}

void color_field::update()
{
    m_changed = false;
    m_committed = false;

    // Switched off with the picker up: the picker goes. What it already changed stays changed - it was
    // reported as it happened - and the commit below still closes the gesture.
    if(!m_enabled && m_popup.opened()) {
        m_popup.close();
    }

    ImGui::PushID(m_id.c_str());

    const bool fade = kit::fade_disabled(m_enabled);
    if(fade) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * theme::disabled_alpha);
    }

    const float d = m_diameter > 0.f ? m_diameter : ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float label_w = m_label.empty() ? 0.f : ImGui::CalcTextSize(m_label.c_str()).x;
    const float row_w =
        m_swatch_first ? d + (label_w > 0.f ? k_label_gap + label_w : 0.f) : (std::max)(d, ImGui::GetContentRegionAvail().x);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Default: the swatch pinned to the right edge, the same way every label/control row in the overlay is
    // laid out (paint::label_row). Swatch-first: at the left, the label after it.
    const ImVec2 swatch_min { m_swatch_first ? pos.x : pos.x + row_w - d, pos.y };
    const ImVec2 swatch_max { swatch_min.x + d, swatch_min.y + d };

    if(!m_label.empty()) {
        const float text_x = m_swatch_first ? swatch_max.x + k_label_gap : pos.x;
        const float text_max = m_swatch_first ? pos.x + row_w : swatch_min.x - k_label_gap;
        const ImVec2 text_pos { text_x, pos.y + (d - ImGui::GetFontSize()) * 0.5f };
        kit::paint::add_text_ellipsis(draw, text_pos, text_max, kit::paint::to_u32(theme::text_secondary), m_label.c_str());
    }

    const kit::interaction pointer =
        kit::interact(ImRect { swatch_min, swatch_max }, ImGui::GetID("##swatch"), kit::placement::overlay, m_enabled);

    // On the press, not the release, and under the swatch rather than at the pointer: the picker belongs
    // to this colour, and where on the swatch the click landed is an accident.
    const bool hovered = pointer.hovered;
    if(pointer.left_down) {
        m_picker.set_color(m_color);
        m_popup.set_title(m_label);
        m_popup.open_at(swatch_min, swatch_max);
    }

    m_hover.set(hovered);
    const float hover_t = m_hover.step();

    // Glow takes the swatch's own hue so the hover reads as "this colour", not as a generic accent.
    kit::paint::color_swatch(draw, ImVec2 { swatch_min.x + d * 0.5f, swatch_min.y + d * 0.5f }, d, m_color, hover_t);

    // The swatch is a hit area at a computed place (placement::overlay); the row is claimed here, so the next
    // field lands under it.
    ImGui::Dummy(ImVec2 { row_w, d });

    // Before the popup: it is a window of its own, and a closing one fading out is not part of this row.
    if(fade) {
        ImGui::PopStyleVar();
    }

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
