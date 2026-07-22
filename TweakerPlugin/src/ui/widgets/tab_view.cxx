#include "ui/widgets/tab_view.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

#include <cassert>

namespace
{
constexpr float k_tab_pad_x = 12.f;
constexpr float k_tab_height = 36.f;
constexpr float k_tab_gap = 2.f;
constexpr float k_nav_pad = 6.f;
constexpr float k_default_w = 400.f;
constexpr float k_default_h = 280.f;
constexpr float k_content_pad = 8.f;

constexpr int k_hover_ms = 150;
constexpr int k_press_ms = 90;
constexpr int k_select_ms = 150;
constexpr int k_content_ms = 200;
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
        m_tab_states[i].select_t = selected ? 1.f : 0.f;
        m_tab_states[i].was_selected = selected;
    }
}

void tab_view::retarget_content(float target)
{
    m_content_tween = tweeny::from(m_content_t).to(target).during(k_content_ms).via(tweeny::easing::cubicOut);
}

void tab_view::select_tab(int index)
{
    if(index < 0 || index >= static_cast<int>(m_labels.size())) {
        return;
    }
    if(index == m_selected_tab) {
        return;
    }

    m_previous_tab = m_selected_tab;
    m_selected_tab = index;
    m_selection_changed = true;
    m_content_t = 0.f;
    retarget_content(1.f);
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

    m_widget_size = detail::resolve_size(m_size, k_default_w, k_default_h);
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
    detail::add_rect_filled_rounded(draw, outer.Min, outer.Max, detail::to_u32(theme::surface_muted), m_rounding);
    draw->AddRect(outer.Min, outer.Max, detail::to_u32(theme::border), m_rounding);

    const auto dt = detail::dt_ms();
    if(!m_content_tween.isFinished()) {
        m_content_t = m_content_tween.step(dt);
    }
    if(m_content_t >= 0.999f && m_previous_tab >= 0) {
        m_content_t = 1.f;
        m_previous_tab = -1;
    }

    const float tab_w = m_nav_width - k_nav_pad * 2.f;
    float tab_y = m_origin.y + k_nav_pad;

    for(std::size_t i = 0; i < m_labels.size(); ++i) {
        tab_button_state& st = m_tab_states[i];
        const bool selected = static_cast<int>(i) == m_selected_tab;

        ImGui::PushID(static_cast<int>(i));

        const ImVec2 tab_min { m_origin.x + k_nav_pad, tab_y };
        const ImVec2 tab_max { tab_min.x + tab_w, tab_min.y + k_tab_height };
        const ImRect bb { tab_min, tab_max };

        // Absolute hit targets — parent space claimed later via Dummy(m_widget_size).
        const ImGuiID imgui_id = ImGui::GetID("##tab");
        ImGui::ItemAdd(bb, imgui_id);

        bool hovered = false;
        bool held = false;
        const bool clicked = ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);
        if(clicked) {
            select_tab(static_cast<int>(i));
        }

        if(hovered != st.was_hovered) {
            st.hover_tween = tweeny::from(st.hover_t).to(hovered ? 1.f : 0.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
        }
        if(held != st.was_held) {
            st.press_tween = tweeny::from(st.press_t).to(held ? 1.f : 0.f).during(k_press_ms).via(tweeny::easing::quadraticOut);
        }
        if(selected != st.was_selected) {
            st.select_tween = tweeny::from(st.select_t).to(selected ? 1.f : 0.f).during(k_select_ms).via(tweeny::easing::cubicOut);
        }
        st.was_hovered = hovered;
        st.was_held = held;
        st.was_selected = selected;

        if(!st.hover_tween.isFinished()) {
            st.hover_t = st.hover_tween.step(dt);
        }
        if(!st.press_tween.isFinished()) {
            st.press_t = st.press_tween.step(dt);
        }
        if(!st.select_tween.isFinished()) {
            st.select_t = st.select_tween.step(dt);
        }

        if(st.select_t > 0.01f) {
            ImVec4 bg = theme::accent_soft;
            bg.w *= st.select_t;
            detail::add_rect_filled_rounded(draw, tab_min, tab_max, detail::to_u32(bg), m_rounding);
        }

        const char* text = m_labels[i].c_str();
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        const float scale = 1.f - 0.04f * st.press_t;
        const float text_h = text_size.y * scale;
        const ImVec2 text_pos {
            tab_min.x + k_tab_pad_x,
            tab_min.y + (k_tab_height - text_h) * 0.5f,
        };

        ImVec4 text_col = detail::lerp(theme::text_secondary, theme::text_primary, st.hover_t);
        text_col = detail::lerp(text_col, theme::text_primary, st.select_t);

        ImVec4 glow_col = detail::lerp(theme::accent_primary, theme::accent_text, st.select_t);
        glow_col.w *= st.select_t;

        if(st.select_t > 0.01f) {
            detail::add_text_glow(draw, text_pos, text, detail::to_u32(text_col), glow_col, st.select_t);
        }
        else {
            draw->AddText(text_pos, detail::to_u32(text_col), text);
        }

        ImGui::PopID();
        tab_y += k_tab_height + k_tab_gap;
    }

    const float content_w = m_widget_size.x > m_nav_width ? m_widget_size.x - m_nav_width : 0.f;

    // Nav column claims left width; content child sits on the same line — one layout pass, no
    // trailing full-size Dummy (that was double-counting height and forcing parent scroll).
    ImGui::SetCursorScreenPos(m_origin);
    ImGui::Dummy(ImVec2 { m_nav_width, m_widget_size.y });
    ImGui::SameLine(0.f, 0.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::surface);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border_subtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { k_content_pad, k_content_pad });

    m_child_begun = true;
    m_child_visible = ImGui::BeginChild("##content",
        ImVec2 { content_w, m_widget_size.y },
        ImGuiChildFlags_Borders,
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
    const bool is_previous = tab_index == m_previous_tab && m_previous_tab >= 0;
    if(!is_selected && !is_previous) {
        return false;
    }

    float alpha = 1.f;
    if(m_previous_tab >= 0 && m_content_t < 1.f) {
        if(is_selected) {
            alpha = m_content_t;
        }
        else {
            alpha = 1.f - m_content_t;
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

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.f, 0.f });
    m_page_begun = true;
    const bool page_visible = ImGui::BeginChild("##page", m_page_size, ImGuiChildFlags_None, page_flags);
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
        ImGui::EndChild();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

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
