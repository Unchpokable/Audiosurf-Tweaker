#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <string>
#include <string_view>

namespace tw::ui::widgets
{
class button {
public:
    explicit button(const char* id, ImVec2 size = { 120.f, 32.f });

    void set_size(ImVec2 size) noexcept;
    void set_label(std::string_view label);

    // An ui/image/svg resource key (e.g. "icons/play.svg"); empty clears it. Drawn to the left of
    // the label, or alone and centred when there is no label - which is what the Quick Player
    // transport uses. The key is stored, not the resolved image: an ImTextureID only survives until
    // the render device is replaced, so it has to be looked up at draw time (see ui/image/svg.hxx).
    void set_icon(std::string_view resource_key);

    // If label is non-null, overrides stored label for this frame (ImGui::Button style).
    void update(const char* label = nullptr);

    [[nodiscard]] bool clicked() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    void retarget_hover(float target);
    void retarget_press(float target);

    std::string m_id;
    std::string m_label;
    std::string m_icon_key;
    ImVec2 m_size;

    bool m_hovered = false;
    bool m_held = false;
    bool m_was_hovered = false;
    bool m_was_held = false;
    bool m_clicked = false;

    float m_hover_t = 0.f;
    float m_press_t = 0.f;
    tweeny::tween<float> m_hover_tween {};
    tweeny::tween<float> m_press_tween {};
};
} // namespace tw::ui::widgets
