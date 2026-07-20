#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <string>
#include <string_view>

namespace tw::ui::widgets
{
class button
{
public:
    explicit button(const char* id, ImVec2 size = {120.f, 32.f});

    void set_size(ImVec2 size) noexcept;
    void set_label(std::string_view label);

    // If label is non-null, overrides stored label for this frame (ImGui::Button style).
    void update(const char* label = nullptr);

    [[nodiscard]] bool clicked() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    void retarget_hover(float target);
    void retarget_press(float target);

    std::string m_id;
    std::string m_label;
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
