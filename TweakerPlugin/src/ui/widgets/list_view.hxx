#pragma once

#include "ui/widgets/list_item.hxx"

#include <imgui.h>

#include <span>
#include <string>
#include <vector>

namespace tw::ui::widgets
{
class list_view {
public:
    explicit list_view(const char* id, ImVec2 size = { 0.f, 200.f });

    void set_size(ImVec2 size) noexcept;
    void set_items(std::span<const list_item_content> items);

    // Programmatic selection, for when the authoritative choice lives outside the widget (the Quick
    // Player tab adopts whichever playlist the host is positioned in). Out-of-range clears the
    // selection; never raises selection_changed(), which stays reserved for the user's own clicks.
    void set_selected(int index) noexcept;

    void update();

    [[nodiscard]] int selected_index() const noexcept;
    [[nodiscard]] bool selection_changed() const noexcept;

private:
    std::string m_id;
    ImVec2 m_size;
    std::vector<list_item_content> m_items;
    std::vector<list_item> m_rows;
    std::vector<std::string> m_row_ids;

    float m_row_height = 36.f; // grows when any item carries a subtext - see set_items

    int m_selected_index = -1;
    bool m_selection_changed = false;
};
} // namespace tw::ui::widgets
