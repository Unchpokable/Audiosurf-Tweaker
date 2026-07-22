#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <string>

namespace tw::ui::widgets
{
struct list_item_content {
    std::string text;
    ImTextureID icon = ImTextureID_Invalid;
    ImVec2 icon_size = { 0.f, 0.f }; // source px; 0 = fit slot as square
};

class list_item {
public:
    explicit list_item(const char* id, ImVec2 size = { 0.f, 36.f });

    void set_size(ImVec2 size) noexcept;
    void set_content(list_item_content content);
    void set_selected(bool selected) noexcept;

    void update();
    void update(const list_item_content& content);

    [[nodiscard]] bool selected() const noexcept;
    [[nodiscard]] bool clicked() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    void retarget_hover(float target);
    void retarget_select(float target);
    void draw_frame(const list_item_content& content);

    std::string m_id;
    list_item_content m_content;
    ImVec2 m_size;

    bool m_selected = false;
    bool m_hovered = false;
    bool m_held = false;
    bool m_was_hovered = false;
    bool m_was_selected = false;
    bool m_clicked = false;

    float m_hover_t = 0.f;
    float m_select_t = 0.f;
    tweeny::tween<float> m_hover_tween {};
    tweeny::tween<float> m_select_tween {};
};
} // namespace tw::ui::widgets
