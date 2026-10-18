#pragma once

#include <imgui.h>

#include <string>
#include <string_view>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
class button {
public:
    explicit button(const char* id, ImVec2 size = { 120.f, 32.f });

    void set_size(ImVec2 size) noexcept;
    void set_label(std::string_view label);

    // As set; an axis at 0 means "the default" (see update), so only a fixed-size button can be laid
    // out against this (kit::paint::label_row).
    [[nodiscard]] ImVec2 size() const noexcept;

    // An ui/image/svg resource key (e.g. "icons/play.svg"); empty clears it. Drawn to the left of
    // the label, or alone and centred when there is no label - which is what the Quick Player
    // transport uses. The key is stored, not the resolved image: an ImTextureID only survives until
    // the render device is replaced, so it has to be looked up at draw time (see ui/image/svg.hxx).
    void set_icon(std::string_view resource_key);

    // A disabled button does not hover, press or click, and is drawn faded (theme::disabled_alpha) -
    // for "there is nothing for this to do right now", which a live-looking button hides (R12).
    void set_enabled(bool enabled) noexcept;

    // If label is non-null, overrides stored label for this frame (ImGui::Button style).
    void update(const char* label = nullptr);

    [[nodiscard]] bool clicked() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    std::string m_id;
    std::string m_label;
    std::string m_icon_key;
    ImVec2 m_size;

    bool m_hovered = false;
    bool m_held = false;
    bool m_clicked = false;
    bool m_enabled = true;

    kit::anim::channel m_hover { theme::motion::hover };
    kit::anim::channel m_press { theme::motion::press };
};
} // namespace tw::ui::widgets
