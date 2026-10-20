#pragma once

#include "ui/widgets/list_item.hxx"

#include <imgui.h>
#include <imgui_internal.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A page of controls described as data: an owner adds nodes - a toggle, a slider, a group with more inside -
// and the form keeps one widget per node, draws them every frame, and reports what the user changed.
//
// Made for the overlay tabs scripts declare (Docs/Internal/script-tabs.md §3.3), and knows nothing of
// Lua: it is the part that makes stateful widgets work for an owner that is not C++ and does not draw. The
// owner holds **the value** of every node - a double, whatever the kind - and the widget is its view:
//
//  - every frame the model's value goes into the widget through its programmatic setter (set_checked,
//    set_value, ...), which is a no-op when nothing changed and never raises changed() - that contract is
//    what lets the model be pushed in every frame without looping against the user's click;
//  - after update(), a change the user made comes back out into the model and into events().
//
// Writes from the owner lose against a gesture in progress: a slider or a colour being dragged ignores its
// setter, and the next changed() overwrites the model. The user is the one holding the mouse.
//
// Node ids are indices, stable for the form's life - which is the owner's: a script that reloads builds a new
// form. Nothing here allocates on a frame except the widgets themselves where they always do.
namespace tw::ui::form
{
// What a node is. The numbering is ABI - the Lua layer passes it as a number - so append, never renumber.
enum class kind : int {
    group = 0,     // item_group across the page; its children are drawn inside it
    toggle = 1,    // value 0/1
    slider = 2,    // value in [min, max], fractional
    number = 3,    // value an integer in [min, max]
    choice = 4,    // value the index of the selected option, 0-based
    color = 5,     // value a packed colour, IM_COL32 order (0xAABBGGRR) - what tw.rgb and tw.hud.* use
    button = 6,    // no value; a click is an event
    text = 7,      // a paragraph; its label is the text
    separator = 8, // a rule; no label
    list = 9,      // value the selected row, -1 for none
};

inline constexpr int k_kind_count = 10;

// One thing the user did this frame. `committed`: the end of a gesture (a slider released, a colour picker's
// drag finished) - or always, for a control that has no gestures.
struct event {
    int node;
    double value;
    bool committed;
};

// Groups inside groups go this deep and no deeper: a section, and a sub-section in it. A frame in a frame in
// a frame is a page that has lost its structure, and every level eats inner padding off the width.
inline constexpr int k_max_group_depth = 2;

class form {
public:
    // `id` scopes every ImGui id in the form - two forms in one window must differ.
    explicit form(std::string_view id);
    ~form();

    form(const form&) = delete;
    form& operator=(const form&) = delete;

    // A new node, last among its parent's children; `parent` -1 for the top level, otherwise a group.
    // Returns its id, or -1: the parent is not a group, or is nested k_max_group_depth deep already.
    int add(kind what, int parent, std::string_view label);

    [[nodiscard]] int size() const noexcept;
    [[nodiscard]] bool valid(int node) const noexcept;
    [[nodiscard]] kind kind_of(int node) const noexcept;

    // slider, number: the range, which also clamps the value. min > max is taken as min = max. Defaults:
    // 0..1 for a slider, 0..100 for a number.
    void set_range(int node, double min_value, double max_value);

    // choice: the options, which clamps the selection into the new count.
    void set_options(int node, std::span<const std::string_view> labels);

    // list: the rows, which drops a selection past the new end; and the list's height in pixels (default 160).
    void set_items(int node, std::span<const widgets::list_item_content> items);
    void set_height(int node, float height);

    // The owner's value, normalized to the kind (a toggle to 0/1, a number rounded, everything clamped). Never
    // an event. No-op on a node without a value.
    void set_value(int node, double value);
    [[nodiscard]] double value(int node) const noexcept;

    // The label - the text of a text node, the title of a group. Compared before it is copied, so an owner
    // that sets the same text every frame costs a comparison.
    void set_label(int node, std::string_view label);

    // Shown when the pointer rests on the node's row. Empty for none.
    void set_tooltip(int node, std::string_view tooltip);

    // A disabled node shows its value and takes no input; a disabled group disables what is in it. A hidden
    // node takes no room at all, and a hidden group hides what is in it.
    void set_enabled(int node, bool enabled);
    void set_visible(int node, bool visible);

    // Draws the form where the cursor is, as wide as the space left. Clears and refills events().
    void draw();

    // What the user changed during the last draw(), in the order it happened. At most one per node.
    [[nodiscard]] std::span<const event> events() const noexcept;

    // The narrowest page the form fits on, as of the last draw(): what a control that does not shrink - a strip
    // of choice chips - needs, plus the group frames around it. The menu holds its window at least this wide
    // while the form's tab is open, the way it does for the Player tab.
    [[nodiscard]] float min_width() const noexcept;

    // Where the node's row was on the last draw(), in screen space; empty when it was not drawn. For whoever
    // has to point at a row it did not lay out itself - the snapshot stand aiming its scripted input.
    [[nodiscard]] ImRect last_rect(int node) const noexcept;

private:
    struct node;

    void draw_children(const std::vector<int>& children);
    void draw_node(node& n);
    [[nodiscard]] float need_width(const node& n) const;
    void push_event(int id, double value, bool committed);

    std::string m_id;
    std::vector<std::unique_ptr<node>> m_nodes;
    std::vector<int> m_roots;
    std::vector<event> m_events;

    // The row the pointer rests on, and since when - one per form, because one row is ever under the pointer.
    int m_hover_node = -1;
    double m_hover_since = 0.0;
    bool m_hover_claimed = false; // some row took the pointer this frame

    float m_page_width = 0.f; // the width draw() started with
    float m_min_width = 0.f;  // being measured during draw(), the answer after it
    float m_measured_min = 0.f;
};

// ImGui's BeginDisabled() at theme::disabled_alpha, the alpha every control dims to on its own (R12) - for a
// subtree switched off as a whole. Nested inside another disabled scope it does not dim twice, as ImGui's own.
void begin_disabled();
void end_disabled();
} // namespace tw::ui::form
