#include "ui/widgets/tab_view.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cassert>

namespace
{
constexpr float k_tab_pad_x = 12.f;
constexpr float k_tab_height = 36.f;
constexpr float k_tab_gap = 2.f;
constexpr float k_nav_pad = tw::ui::theme::space::nav_pad;
constexpr float k_default_w = 400.f;
constexpr float k_default_h = 280.f;
constexpr float k_content_pad = tw::ui::theme::space::content_pad;

// The page clips at its own edge, padding included: a slider or a colour swatch at the edge of the content
// keeps its whole glow only while the page's padding (this, less the content's 1-px border) is at least as
// wide as the glow reaches.
static_assert(k_content_pad - 1.f >= tw::ui::kit::paint::glow_reach);
} // namespace

namespace tw::ui::widgets
{
tab_view::tab_view(const char* id, ImVec2 size) : m_id(id ? id : "tab_view"), m_size(size)
{
}

void tab_view::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void tab_view::set_rounding(float rounding) noexcept
{
    m_rounding = rounding;
}

void tab_view::set_tabs(std::span<const std::string_view> labels)
{
    m_labels.clear();
    m_tab_states.clear();
    m_labels.reserve(labels.size());
    m_tab_states.reserve(labels.size());

    for(const std::string_view label : labels) {
        m_labels.emplace_back(label.begin(), label.end());
        m_tab_states.emplace_back();
    }

    if(m_labels.empty()) {
        m_selected_tab = -1;
        m_previous_tab = -1;
        return;
    }

    if(m_selected_tab < 0 || m_selected_tab >= static_cast<int>(m_labels.size())) {
        m_selected_tab = 0;
    }

    if(m_previous_tab >= static_cast<int>(m_labels.size())) {
        m_previous_tab = -1;
    }

    for(std::size_t i = 0; i < m_tab_states.size(); ++i) {
        const bool selected = static_cast<int>(i) == m_selected_tab;
        m_tab_states[i].select.snap(selected ? 1.f : 0.f);
    }
}

void tab_view::select_tab(int index)
{
    if(index < 0 || index >= static_cast<int>(m_labels.size())) {
        return;
    }
    if(index == m_selected_tab) {
        return;
    }

    // Two pages are drawn during a transition: the selected one at content_t, the previous at 1 - content_t.
    // A switch in the middle of one must not make either jump (R4).
    const float t = m_content.value();
    const bool in_transition = m_previous_tab >= 0 && t < 1.f;

    if(in_transition && index == m_previous_tab) {
        // Back to the page being left: the two swap roles, and the curve restarts from where that page is
        // now, so both carry on from their current opacity.
        m_previous_tab = m_selected_tab;
        m_selected_tab = index;
        m_content.restart(1.f - t, 1.f);
    }
    else {
        // A third page. Only one of the two showing can stay as the one fading out - the more visible
        // one; the other goes at once. The jump is under half the opacity, where restarting blindly made
        // the page being left pop back to full.
        if(!in_transition || t >= 0.5f) {
            m_previous_tab = m_selected_tab;
        }
        m_selected_tab = index;
        m_content.restart(0.f, 1.f);
    }

    m_selection_changed = true;
}

void tab_view::set_selected_tab(int index)
{
    assert(!m_active && "tab_view::set_selected_tab() must be called outside begin()/end()");
    select_tab(index);
}

void tab_view::begin()
{
    assert(!m_active && "tab_view::begin() called while already active");
    m_active = true;
    m_selection_changed = false;
    m_view_open = false;
    m_page_begun = false;
    m_child_begun = false;
    m_child_visible = false;

    ImGui::PushID(m_id.c_str());

    m_widget_size = kit::paint::resolve_size(m_size, k_default_w, k_default_h);
    m_origin = ImGui::GetCursorScreenPos();

    float max_label_w = 0.f;
    for(const std::string& label : m_labels) {
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        if(ts.x > max_label_w) {
            max_label_w = ts.x;
        }
    }
    m_nav_width = max_label_w + k_tab_pad_x * 2.f + k_nav_pad * 2.f;
    if(m_nav_width > m_widget_size.x * 0.5f) {
        m_nav_width = m_widget_size.x * 0.5f;
    }

    const ImRect outer {
        m_origin,
        ImVec2 { m_origin.x + m_widget_size.x, m_origin.y + m_widget_size.y },
    };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(outer.Min, outer.Max, kit::paint::to_u32(theme::surface_muted), m_rounding);
    draw->AddRect(outer.Min, outer.Max, kit::paint::to_u32(theme::border), m_rounding);

    if(m_content.step() >= 0.999f && m_previous_tab >= 0) {
        m_content.snap(1.f);
        m_previous_tab = -1;
    }

    // The navigation column is a child window of its own (R8): a package of sky parameters can declare more
    // groups than fit, and the column then scrolls under the wheel (no bar - it would eat the labels'
    // width). Same padding and spacing as the tabs always had, so a column that fits looks as before.
    ImGui::SetCursorScreenPos(m_origin);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { k_nav_pad, k_nav_pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    const bool nav_visible = ImGui::BeginChild("##nav",
        ImVec2 { m_nav_width, m_widget_size.y },
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if(nav_visible) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { ImGui::GetStyle().ItemSpacing.x, k_tab_gap });
        ImDrawList* nav_draw = ImGui::GetWindowDrawList();
        const float tab_w = m_nav_width - k_nav_pad * 2.f;
        const float label_w = tab_w - k_tab_pad_x * 2.f;

        // Concentric with the frame's corner (theme::radius): its radius less the gap to it.
        const float tab_rounding = (std::max)(0.f, m_rounding - k_nav_pad);

        for(std::size_t i = 0; i < m_labels.size(); ++i) {
            tab_button_state& st = m_tab_states[i];
            const bool selected = static_cast<int>(i) == m_selected_tab;

            ImGui::PushID(static_cast<int>(i));

            const ImVec2 tab_min = ImGui::GetCursorScreenPos();
            const ImVec2 tab_max { tab_min.x + tab_w, tab_min.y + k_tab_height };

            const kit::interaction pointer = kit::interact(ImRect { tab_min, tab_max }, ImGui::GetID("##tab"));
            if(pointer.clicked) {
                select_tab(static_cast<int>(i));
            }

            // `selected` was read before the click above was handled, so a clicked tab starts its selection
            // curve on the next frame - as it always has. No press state: a tab's press used to shrink the
            // text's measured height by up to 4%, which moved it by under a pixel and read as nothing (R4).
            st.hover.set(pointer.hovered);
            st.select.set(selected);
            const float hover_t = st.hover.step();
            const float select_t = st.select.step();

            if(select_t > 0.01f) {
                ImVec4 bg = theme::accent_soft;
                bg.w *= select_t;
                nav_draw->AddRectFilled(tab_min, tab_max, kit::paint::to_u32(bg), tab_rounding);
            }

            // Cut with an ellipsis to the tab's width (R8) - and cut once per width, not per frame: the cut
            // allocates, and the glow below needs the cut text as a string.
            if(st.fitted_for != label_w) {
                st.fitted = kit::paint::fit_text(m_labels[i].c_str(), label_w);
                st.fitted_for = label_w;
            }
            const char* text = st.fitted.c_str();
            const ImVec2 text_pos { tab_min.x + k_tab_pad_x, tab_min.y + (k_tab_height - ImGui::GetFontSize()) * 0.5f };

            ImVec4 text_col = kit::paint::lerp(theme::text_secondary, theme::text_primary, hover_t);
            text_col = kit::paint::lerp(text_col, theme::text_primary, select_t);

            ImVec4 glow_col = kit::paint::lerp(theme::accent_primary, theme::accent_text, select_t);
            glow_col.w *= select_t;

            if(select_t > 0.01f) {
                kit::paint::add_text_glow(nav_draw, text_pos, text, kit::paint::to_u32(text_col), glow_col, select_t);
            }
            else {
                nav_draw->AddText(text_pos, kit::paint::to_u32(text_col), text);
            }

            ImGui::PopID();
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    const float content_w = m_widget_size.x > m_nav_width ? m_widget_size.x - m_nav_width : 0.f;

    // The content child sits on the nav column's line - one layout pass.
    ImGui::SameLine(0.f, 0.f);

    // Padding here only as wide as the border, so the page sits just inside it; the rest of the padding
    // belongs to the page (begin_view). A child clips to its own rect, padding included, so padding outside
    // the page would put the page's clip edge right where its content starts, and the glow of a slider or a
    // colour swatch at that edge would be cut off in a straight line.
    //
    // The frame's rounding is the tab_view's own: the content shares its right-hand corners with the outer
    // frame, and two radii on one corner would not meet.
    const float border = ImGui::GetStyle().ChildBorderSize;
    m_page_pad = (std::max)(k_content_pad - border, 0.f);

    m_child_begun = true;
    m_child_visible = kit::paint::scroll_body_begin("##content",
        ImVec2 { content_w, m_widget_size.y },
        border,
        m_rounding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if(m_child_visible) {
        // Fixed page slot so overlapping crossfade views share one layout size.
        m_page_pos = ImGui::GetCursorPos();
        m_page_size = ImGui::GetContentRegionAvail();
    }
    else {
        m_page_pos = ImVec2 { 0.f, 0.f };
        m_page_size = ImVec2 { 0.f, 0.f };
    }
}

bool tab_view::begin_view(int tab_index)
{
    assert(m_active && "tab_view::begin_view() without begin()");
    assert(!m_view_open && "tab_view::begin_view() while a view is already open");

    if(!m_child_visible) {
        return false;
    }
    if(tab_index < 0 || tab_index >= static_cast<int>(m_labels.size())) {
        return false;
    }

    const bool is_selected = tab_index == m_selected_tab;
    const bool is_previous = tab_index == m_previous_tab;
    if(!is_selected && !is_previous) {
        return false;
    }

    float alpha = 1.f;
    const float content_t = m_content.value();
    if(m_previous_tab >= 0 && content_t < 1.f) {
        if(is_selected) {
            alpha = content_t;
        }
        else {
            alpha = 1.f - content_t;
        }
    }
    else if(!is_selected) {
        return false;
    }

    ImGui::PushID(tab_index);
    ImGui::SetCursorPos(m_page_pos);

    // Isolated page: fixed size, no host scroll. Outgoing ignores input so the
    // incoming list_view keeps a stable scroll target during crossfade.
    ImGuiWindowFlags page_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
    if(is_previous) {
        page_flags |= ImGuiWindowFlags_NoInputs;
    }

    // AlwaysUseWindowPadding: a child without a border ignores WindowPadding otherwise.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { m_page_pad, m_page_pad });
    m_page_begun = true;
    const bool page_visible = ImGui::BeginChild("##page", m_page_size, ImGuiChildFlags_AlwaysUseWindowPadding, page_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if(!page_visible) {
        ImGui::EndChild();
        ImGui::PopID();
        m_page_begun = false;
        return false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    m_view_open = true;
    return true;
}

void tab_view::end_view()
{
    assert(m_view_open && "tab_view::end_view() without matching begin_view()");
    ImGui::PopStyleVar(); // Alpha
    assert(m_page_begun);
    ImGui::EndChild();    // ##page
    ImGui::PopID();
    m_page_begun = false;
    m_view_open = false;
}

void tab_view::end()
{
    assert(m_active && "tab_view::end() without begin()");
    assert(!m_view_open && "tab_view::end() with unclosed begin_view()");

    if(m_child_visible) {
        // One layout item for the overlapping pages — avoids SetCursorPos extend warning
        // when no page was submitted (or cursor was reset between pages).
        ImGui::SetCursorPos(m_page_pos);
        ImGui::Dummy(m_page_size);
    }

    if(m_child_begun) {
        kit::paint::scroll_body_end();
    }

    ImGui::PopID();

    m_child_begun = false;
    m_child_visible = false;
    m_page_begun = false;
    m_active = false;
}

int tab_view::selected_tab() const noexcept
{
    return m_selected_tab;
}

bool tab_view::selection_changed() const noexcept
{
    return m_selection_changed;
}
} // namespace tw::ui::widgets
