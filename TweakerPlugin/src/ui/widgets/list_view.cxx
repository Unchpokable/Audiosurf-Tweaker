#include "ui/widgets/list_view.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

namespace
{
constexpr float k_row_height = 36.f;

// Rows carrying a second line need the height for it. One height for the whole list, decided by
// whether any item has a subtext at all: a list where only some rows were taller would read as
// misaligned rather than as informative.
constexpr float k_row_height_two_line = 46.f;
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

    m_row_height = k_row_height;
    for(const list_item_content& item : m_items) {
        if(!item.subtext.empty()) {
            m_row_height = k_row_height_two_line;
            break;
        }
    }

    // The highlights are keyed by index and survive the new items: a row that stayed where it was stays
    // lit. Only an index past the end is let go.
    if(m_selected_index >= static_cast<int>(m_items.size())) {
        m_selected_index = -1;
        m_select.jump(-1);
    }
    if(m_hover.current() >= static_cast<int>(m_items.size())) {
        m_hover.reset();
    }
}

void list_view::set_selected(int index) noexcept
{
    m_selected_index = index >= 0 && index < static_cast<int>(m_items.size()) ? index : -1;
    if(m_select.current() != m_selected_index) {
        m_select.jump(m_selected_index);
    }
}

void list_view::update()
{
    m_selection_changed = false;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = kit::paint::resolve_size(m_size, 200.f, 200.f);

    if(kit::paint::scroll_body_begin("##list", size, theme::space::list_pad, theme::radius::scroll)) {
        // Stepped once for the list, not per row: one hover and one selection are ever in flight.
        m_hover.step();
        m_select.step();
        bool hover_seen = false;

        // The gap between the cards is the item spacing, and only it.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { ImGui::GetStyle().ItemSpacing.x, theme::space::row_gap });
        ImDrawList* draw = ImGui::GetWindowDrawList();

        for(std::size_t i = 0; i < m_items.size(); ++i) {
            const int index = static_cast<int>(i);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 max { pos.x + ImGui::GetContentRegionAvail().x, pos.y + m_row_height };

            ImGui::PushID(index);
            const kit::interaction pointer = kit::interact(ImRect { pos, max }, ImGui::GetID("##row"));
            ImGui::PopID();
            if(!pointer.visible) {
                continue;
            }

            if(pointer.hovered) {
                hover_seen = true;
                m_hover.focus(index);
            }
            if(pointer.clicked && m_selected_index != index) {
                m_selected_index = index;
                m_selection_changed = true;
                m_select.focus(index);
            }

            kit::paint::row_background(draw, pos, max, m_hover.alpha(index), m_select.alpha(index));
            list_item::draw_content(draw, pos, max, m_items[i]);
        }

        ImGui::PopStyleVar();

        // A row under the pointer always reports it, so no report means the pointer left the list.
        if(!hover_seen) {
            m_hover.focus(-1);
        }
    }
    kit::paint::scroll_body_end();

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
