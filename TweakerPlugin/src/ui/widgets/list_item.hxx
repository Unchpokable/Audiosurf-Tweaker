#pragma once

#include <imgui.h>

#include <string>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
struct list_item_content {
    std::string text;

    // Optional second line, drawn under `text` in a smaller, muted type - a playlist's track count
    // under its name. Empty means a single-line row, which is what every existing caller gets.
    std::string subtext;

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

    // What a row shows inside its card - icon, text, the smaller second line - for a list that draws its
    // rows itself (list_view) and so looks exactly like one made of list_items.
    static void draw_content(ImDrawList* draw, const ImVec2& min, const ImVec2& max, const list_item_content& content);

private:
    void draw_frame(const list_item_content& content);

    std::string m_id;
    list_item_content m_content;
    ImVec2 m_size;

    bool m_selected = false;
    bool m_hovered = false;
    bool m_held = false;
    bool m_clicked = false;

    kit::anim::channel m_hover { theme::motion::hover };
    kit::anim::channel m_select { theme::motion::select };
};
} // namespace tw::ui::widgets
