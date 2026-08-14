#pragma once

#include <imgui.h>

#include <string>

namespace tw::ui::widgets
{
// A clamped integer field with -/+ steppers. Exists for Quick Player's parameterized song tags
// ([as-msz12], [as-wb50], [as-monopt3] and friends), each of which carries its own range straight
// from the tag catalog.
//
// Built on ImGui::InputScalar restyled with theme colours rather than drawn from scratch, the same
// approach color_picker already takes for its RGBA fields: text entry, selection, and keyboard
// editing are a lot of behaviour to reimplement for a field this small, and the chrome is the only
// part that actually had to match.
class number_input {
public:
    explicit number_input(const char* id, float width = 108.f);

    void set_width(float width) noexcept;

    // Values are clamped into range on every write, including the current one - a range that
    // shrinks under a stored value must not leave the widget showing something it cannot produce.
    void set_range(int min_value, int max_value) noexcept;

    // Programmatic set - no-op if unchanged, and never raises changed(), so the owner can push the
    // authoritative value in every frame without looping against the user's own edits.
    void set_value(int value) noexcept;

    void update();

    [[nodiscard]] int value() const noexcept;

    // True on the frame the user committed an edit (a stepper click, or leaving the field after
    // typing). Deliberately not per-keystroke: each change is a wire message on the caller's side.
    [[nodiscard]] bool changed() const noexcept;

private:
    void clamp_value() noexcept;

    std::string m_id;
    float m_width;

    int m_value = 0;
    int m_min = 0;
    int m_max = 0;
    bool m_changed = false;
};
} // namespace tw::ui::widgets
