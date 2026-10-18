#pragma once

#include <imgui.h>

#include <string>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
class toggle {
public:
    explicit toggle(const char* id, ImVec2 size = { 40.f, 22.f });

    void set_size(ImVec2 size) noexcept;

    // As set - what a row lays itself out against (kit::paint::label_row).
    [[nodiscard]] ImVec2 size() const noexcept;

    // Not noexcept: a change starts the thumb's slide, which builds a tween. Before the toggle has been
    // drawn once it snaps instead - a toggle appearing already on is not one being switched on (R5).
    void set_checked(bool checked);

    // The state with no slide, for a change nobody made by hand: the track under a context menu changed
    // and the toggles now show another track's tags (R5).
    void snap(bool checked) noexcept;

    // A disabled toggle does not hover, press or flip, and is drawn faded (theme::disabled_alpha).
    void set_enabled(bool enabled) noexcept;

    void update();

    [[nodiscard]] bool checked() const noexcept;
    [[nodiscard]] bool changed() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    std::string m_id;
    ImVec2 m_size;

    bool m_checked = false;
    bool m_changed = false;
    bool m_hovered = false;
    bool m_held = false;
    bool m_enabled = true;
    bool m_drawn = false;

    kit::anim::channel m_thumb { theme::motion::slide };
    kit::anim::channel m_hover { theme::motion::hover };
    kit::anim::channel m_press { theme::motion::press };
};
} // namespace tw::ui::widgets
