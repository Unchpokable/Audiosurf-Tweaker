#pragma once

#include "ui/widgets/list_item.hxx"

#include <imgui.h>

#include <span>
#include <string>
#include <vector>

namespace tw::ui::widgets
{
class list_view
{
public:
    explicit list_view(const char* id, ImVec2 size = {0.f, 200.f});

    void set_size(ImVec2 size) noexcept;
    void set_items(std::span<const list_item_content> items);

    void update();

    [[nodiscard]] int selected_index() const noexcept;
    [[nodiscard]] bool selection_changed() const noexcept;

private:
    std::string m_id;
    ImVec2 m_size;
    std::vector<list_item_content> m_items;
    std::vector<list_item> m_rows;
    std::vector<std::string> m_row_ids;

    int m_selected_index = -1;
    bool m_selection_changed = false;
};
} // namespace tw::ui::widgets
