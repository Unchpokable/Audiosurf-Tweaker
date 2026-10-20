#pragma once

#include <imgui.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/kit/anim.hxx"

namespace tw::ui::widgets
{
class tab_view {
public:
    explicit tab_view(const char* id, ImVec2 size = { 0.f, 0.f });

    void set_size(ImVec2 size) noexcept;
    void set_rounding(float rounding) noexcept;

    // Replaces the tabs, outside begin()/end(). Every tab's hover and label cache start over; page ids go back
    // to the tab's index (set_tab_id).
    //
    // `selected` -1 keeps the selected index as it was, clamped. Anything else selects that tab **without a
    // transition and without selection_changed()**: the strip was rebuilt under the user - a script's tab came
    // or went - and nobody switched pages. There is no page to fade out from, either: its owner may be gone.
    void set_tabs(std::span<const std::string_view> labels, int selected = -1);

    // One tab's label, outside begin()/end(). A no-op when it is the same text; otherwise only that tab's
    // ellipsis cache is redone.
    void set_label(int index, std::string_view label);

    // A tab drawn dimmed (theme::disabled_alpha) - for a page whose content is switched off as a whole. It is
    // still a tab: it can be selected, and its page shows.
    void set_tab_enabled(int index, bool enabled);

    // What the tab's page is keyed on in ImGui's id stack, instead of its index. For tabs that come and go: a
    // page keyed on its index would hand its scroll position and its widgets' state to whichever tab moves
    // into that index next.
    void set_tab_id(int index, std::string_view id);

    // The navigation column's width as of the last begin() - for an owner sizing the window around a page.
    [[nodiscard]] float nav_width() const noexcept;

    // Programmatic tab switch, animated exactly as a click on the nav button would be. Must be
    // called outside begin()/end(), like set_tabs. Raises selection_changed() the same way a click
    // does - the transition is real either way, and callers that care about "who asked" already
    // know, because they are the ones who asked.
    void set_selected_tab(int index);

    void begin();
    bool begin_view(int tab_index);
    void end_view();
    void end();

    [[nodiscard]] int selected_tab() const noexcept;
    [[nodiscard]] bool selection_changed() const noexcept;

private:
    struct tab_button_state {
        kit::anim::channel hover { theme::motion::hover };
        kit::anim::channel select { theme::motion::select };

        // The label as drawn - cut with an ellipsis to the column - and the width it was cut for.
        std::string fitted;
        float fitted_for = -1.f;

        bool enabled = true;
        ImGuiID page_id = 0; // 0 = the page is keyed on the tab's index
    };

    void select_tab(int index);

    std::string m_id;
    ImVec2 m_size;
    float m_rounding = theme::radius::panel;

    std::vector<std::string> m_labels;
    std::vector<tab_button_state> m_tab_states;

    int m_selected_tab = 0;
    int m_previous_tab = -1;
    bool m_selection_changed = false;

    // 1 when one page is showing; runs 0 -> 1 while the selected page fades in over the previous one.
    kit::anim::channel m_content { theme::motion::transition, 1.f };

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
    float m_page_pad = 0.f;
};
} // namespace tw::ui::widgets
