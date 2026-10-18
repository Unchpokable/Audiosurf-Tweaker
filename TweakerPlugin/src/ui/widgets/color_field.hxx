#pragma once

#include "ui/widgets/color_picker.hxx"
#include "ui/widgets/popup_menu.hxx"

#include <imgui.h>

#include <string>
#include <string_view>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
// One labelled colour: a round swatch on a row, opening the full color_picker in a popup.
//
// The picker is a big widget - SV square, hue and alpha bars, hex and four RGBA fields - and the
// shared sky palette alone declares four colours. Stacked inline that is four screens of picker for
// four values; behind a swatch it is four rows. The Settings tab's theme editor and the Skybox tab's
// palette both use it, so a colour opens the same way everywhere: a left click on the swatch.
//
// changed() and committed() split for the same reason slider does: dragging inside a picker is
// hundreds of colours and only the last one is worth writing to disk.
class color_field {
public:
    explicit color_field(const char* id, float swatch_diameter = 0.f);

    void set_label(std::string_view label);

    // The default row is the overlay's usual "label + control": the label at the left, the swatch at the
    // right edge, the row as wide as the space it is in. Swatch-first is the swatch with the label after
    // it, as wide as the two need - for colours packed into shrink-to-content boxes, like the theme
    // editor's groups, where a row that takes all the width there is would leave no room for a second
    // column.
    void set_swatch_first(bool swatch_first) noexcept;

    // Programmatic set - never raises changed()/committed(), and ignored while the popup is open so
    // an owner echoing its own state back cannot fight the picker.
    void set_color(const ImVec4& rgba) noexcept;

    void update();

    [[nodiscard]] ImVec4 color() const noexcept;
    [[nodiscard]] bool changed() const noexcept;
    [[nodiscard]] bool committed() const noexcept;

private:
    std::string m_id;
    std::string m_label;
    float m_diameter;
    bool m_swatch_first = false;

    ImVec4 m_color { 1.f, 1.f, 1.f, 1.f };

    popup_menu m_popup;
    color_picker m_picker;

    kit::anim::channel m_hover { theme::motion::hover };

    // Set by any picker movement, cleared when the mouse comes up. This is what turns a drag into
    // one commit instead of one per frame.
    bool m_dirty = false;

    bool m_changed = false;
    bool m_committed = false;
};
} // namespace tw::ui::widgets
