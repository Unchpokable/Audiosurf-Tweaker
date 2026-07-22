#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tw::ui::widgets
{
class tab_view {
public:
    explicit tab_view(const char* id, ImVec2 size = { 0.f, 0.f });

    void set_size(ImVec2 size) noexcept;
    void set_rounding(float rounding) noexcept;
    void set_tabs(std::span<const std::string_view> labels);

    void begin();
    bool begin_view(int tab_index);
    void end_view();
    void end();

    [[nodiscard]] int selected_tab() const noexcept;
    [[nodiscard]] bool selection_changed() const noexcept;

private:
    struct tab_button_state {
        float hover_t = 0.f;
        float press_t = 0.f;
        float select_t = 0.f;
        tweeny::tween<float> hover_tween {};
        tweeny::tween<float> press_tween {};
        tweeny::tween<float> select_tween {};
        bool was_hovered = false;
        bool was_held = false;
        bool was_selected = false;
    };

    void retarget_content(float target);
    void select_tab(int index);

    std::string m_id;
    ImVec2 m_size;
    float m_rounding = 5.f;

    std::vector<std::string> m_labels;
    std::vector<tab_button_state> m_tab_states;

    int m_selected_tab = 0;
    int m_previous_tab = -1;
    bool m_selection_changed = false;

    float m_content_t = 1.f;
    tweeny::tween<float> m_content_tween {};

    bool m_active = false;
    bool m_view_open = false;
    bool m_child_begun = false;   // BeginChild was called — EndChild is mandatory
    bool m_child_visible = false; // BeginChild return value (content may be skipped)
    bool m_page_begun = false;
    float m_nav_width = 0.f;
    ImVec2 m_origin {};
    ImVec2 m_widget_size {};
    ImVec2 m_page_pos {};
    ImVec2 m_page_size {};
};
} // namespace tw::ui::widgets
