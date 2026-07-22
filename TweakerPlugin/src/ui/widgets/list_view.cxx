#include "ui/widgets/list_view.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <format>

namespace
{
constexpr float k_row_gap = 4.f;
constexpr float k_row_height = 36.f;
} // namespace

namespace tw::ui::widgets
{
list_view::list_view(const char* id, ImVec2 size) : m_id(id ? id : "list_view"), m_size(size)
{
}

void list_view::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void list_view::set_items(std::span<const list_item_content> items)
{
    m_items.assign(items.begin(), items.end());
    m_rows.clear();
    m_row_ids.clear();
    m_rows.reserve(m_items.size());
    m_row_ids.reserve(m_items.size());

    for(std::size_t i = 0; i < m_items.size(); ++i) {
        m_row_ids.push_back(std::format("row_{}", i));
        m_rows.emplace_back(m_row_ids.back().c_str(), ImVec2 { 0.f, k_row_height });
        m_rows.back().set_content(m_items[i]);
        m_rows.back().set_selected(static_cast<int>(i) == m_selected_index);
    }

    if(m_selected_index >= static_cast<int>(m_items.size())) {
        m_selected_index = -1;
    }
}

void list_view::update()
{
    m_selection_changed = false;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = detail::resolve_size(m_size, 200.f, 200.f);
    const ImGuiChildFlags child_flags = ImGuiChildFlags_Borders;
    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::surface);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 6.f, 6.f });

    if(ImGui::BeginChild("##list", size, child_flags, window_flags)) {
        for(std::size_t i = 0; i < m_rows.size(); ++i) {
            m_rows[i].set_content(m_items[i]);
            m_rows[i].set_selected(static_cast<int>(i) == m_selected_index);
            m_rows[i].set_size(ImVec2 { 0.f, k_row_height });
            m_rows[i].update();

            if(m_rows[i].clicked()) {
                if(m_selected_index != static_cast<int>(i)) {
                    m_selected_index = static_cast<int>(i);
                    m_selection_changed = true;
                }
            }

            if(i + 1 < m_rows.size()) {
                ImGui::Dummy(ImVec2 { 0.f, k_row_gap });
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

int list_view::selected_index() const noexcept
{
    return m_selected_index;
}

bool list_view::selection_changed() const noexcept
{
    return m_selection_changed;
}
} // namespace tw::ui::widgets
