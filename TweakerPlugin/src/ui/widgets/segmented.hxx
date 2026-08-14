#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tw::ui::widgets
{
// A row of mutually exclusive labelled chips - the overlay's counterpart to the desktop's
// RadioButton.QPSegmentButton strip (Quick Player's six playback modes, its Auto/Manual advance
// pair). A cycling icon button was tried for the two-state case on the desktop and failed: two
// states have no unambiguous pictogram, and the "on" state filled the button with accent until the
// icon read as a solid square. Words read; icons did not.
//
// Shrink-to-content in width, like item_group and unlike most widgets here: the caller sets a
// height, and the strip is exactly as wide as its labels need. Chips must not be squeezed to fit a
// panel - the panel is what gets widened (the Player tab stretches the menu window to its content).
class segmented {
public:
    explicit segmented(const char* id, float height = 28.f);

    void set_height(float height) noexcept;
    void set_rounding(float rounding) noexcept;

    // Rebuilds the chips. Selection is kept when it still fits the new option count, otherwise
    // reset to 0. Relatively heavy (per-chip animation state); do not call every frame.
    void set_options(std::span<const std::string_view> labels);

    // Icon-only chips: one ui/image/svg resource key per option, and the labels become tooltip
    // material for the caller rather than anything drawn. What the desktop's playback-mode row does
    // - six words do not fit a transport bar, and six glyphs from one family read as a scale.
    // Keys are stored, not resolved: an ImTextureID dies with its render device (see ui/image/svg.hxx).
    void set_icons(std::span<const std::string_view> resource_keys);

    // Programmatic selection - no-op if unchanged, so it can be called every frame from whatever
    // owns the real value (the same contract as toggle::set_checked). Does not raise changed().
    void set_selected(int index) noexcept;

    void update();

    // Width the strip will occupy, from the same measurement update() uses. Shrink-to-content
    // widgets otherwise cannot be laid out against anything: the Quick Player transport row spreads
    // its leftover width between groups, which means knowing their widths before drawing them.
    [[nodiscard]] float width() const;

    [[nodiscard]] int selected() const noexcept;

    // True only on the frame the *user* picked a different chip - set_selected never sets it, so a
    // caller echoing host state back into the widget cannot loop.
    [[nodiscard]] bool changed() const noexcept;

    // Which chip the mouse is over, or -1. Exposed so the caller can attach its own tooltip: the
    // playback modes need one each ("Single" and "Repeat one" are distinguishable but not
    // self-evident) and the strip has no business owning that text.
    [[nodiscard]] int hovered_index() const noexcept;

private:
    struct chip_state {
        float hover_t = 0.f;
        float select_t = 0.f;
        tweeny::tween<float> hover_tween;
        tweeny::tween<float> select_tween;
        bool was_hovered = false;
        bool was_selected = false;
    };

    std::string m_id;
    float m_height;
    float m_rounding = 8.f;

    [[nodiscard]] float chip_width(std::size_t index) const;

    std::vector<std::string> m_labels;
    std::vector<std::string> m_icon_keys; // empty = text chips
    std::vector<chip_state> m_chips;

    int m_selected = 0;
    int m_hovered_index = -1;
    bool m_changed = false;
};
} // namespace tw::ui::widgets
