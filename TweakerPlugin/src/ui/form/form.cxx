#include "ui/form/form.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/color_field.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/number_input.hxx"
#include "ui/widgets/segmented.hxx"
#include "ui/widgets/slider.hxx"
#include "ui/widgets/toggle.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <variant>

namespace
{
namespace paint = tw::ui::kit::paint;
namespace theme = tw::ui::theme;

using tw::ui::form::kind;

constexpr float k_number_w = 108.f;
constexpr float k_button_h = 28.f;    // the menu's own buttons (Apply, Default)
constexpr float k_button_pad = 16.f;  // label to edge, each side
constexpr float k_button_min_w = 72.f;
constexpr float k_list_h = 160.f;

// A choice wider than this share of the row goes under its label instead of beside it: beside, the label
// would be left a sliver and cut to an ellipsis, and a strip of chips does not shrink (segmented.hxx).
constexpr float k_choice_beside_share = 0.5f;

bool has_value(kind what) noexcept
{
    switch(what) {
        case kind::toggle:
        case kind::slider:
        case kind::number:
        case kind::choice:
        case kind::color:
        case kind::list:
            return true;
        default:
            return false;
    }
}

// The model's colour is IM_COL32-packed; the widget speaks ImVec4. A value conversion, not a colour for
// drawing - no style alpha belongs in it (rule 6 is about what is drawn).
ImVec4 to_rgba(double packed) noexcept
{
    return ImGui::ColorConvertU32ToFloat4(static_cast<ImU32>(packed));
}

double to_packed(const ImVec4& rgba) noexcept
{
    return static_cast<double>(ImGui::ColorConvertFloat4ToU32(rgba));
}
} // namespace

namespace tw::ui::form
{
struct form::node {
    int id = -1;
    kind what = kind::text;
    int depth = 0; // how many groups enclose it

    std::string label;
    std::string tooltip;

    double value = 0.0;
    double min = 0.0;
    double max = 1.0;
    int count = 0; // options of a choice, rows of a list

    bool enabled = true;
    bool visible = true;

    float height = k_list_h; // list
    float button_w = -1.f;   // measured from the label on the next draw; -1 = not yet

    ImRect rect {}; // the row on the last draw

    std::vector<int> children; // group

    std::variant<std::monostate,
        widgets::item_group,
        widgets::toggle,
        widgets::slider,
        widgets::number_input,
        widgets::segmented,
        widgets::color_field,
        widgets::button,
        widgets::list_view>
        widget;
};

form::form(std::string_view id) : m_id(id)
{
}

form::~form() = default;

int form::add(kind what, int parent, std::string_view label)
{
    if(static_cast<int>(what) < 0 || static_cast<int>(what) >= k_kind_count) {
        return -1;
    }

    int depth = 0;
    if(parent >= 0) {
        if(!valid(parent) || m_nodes[static_cast<std::size_t>(parent)]->what != kind::group) {
            return -1;
        }
        depth = m_nodes[static_cast<std::size_t>(parent)]->depth + 1;
    }
    else if(parent != -1) {
        return -1;
    }

    if(what == kind::group && depth >= k_max_group_depth) {
        return -1;
    }

    auto n = std::make_unique<node>();
    n->id = static_cast<int>(m_nodes.size());
    n->what = what;
    n->depth = depth;
    n->label.assign(label);

    // The widget's id is the node's number - stable for the form's life, unique inside its PushID scope.
    const std::string widget_id = "n" + std::to_string(n->id);

    switch(what) {
        case kind::group:
            n->widget.emplace<widgets::item_group>(widget_id.c_str(), n->label.c_str());
            std::get<widgets::item_group>(n->widget).set_fill_width(true);
            break;
        case kind::toggle:
            n->widget.emplace<widgets::toggle>(widget_id.c_str());
            break;
        case kind::slider: {
            auto& w = n->widget.emplace<widgets::slider>(widget_id.c_str());
            w.set_label(n->label);
            w.set_range(0.f, 1.f);
            break;
        }
        case kind::number: {
            n->max = 100.0;
            auto& w = n->widget.emplace<widgets::number_input>(widget_id.c_str(), k_number_w);
            w.set_range(0, 100);
            break;
        }
        case kind::choice:
            n->widget.emplace<widgets::segmented>(widget_id.c_str());
            break;
        case kind::color: {
            n->value = static_cast<double>(IM_COL32_WHITE);
            auto& w = n->widget.emplace<widgets::color_field>(widget_id.c_str());
            w.set_label(n->label);
            break;
        }
        case kind::button: {
            auto& w = n->widget.emplace<widgets::button>(widget_id.c_str());
            w.set_label(n->label);
            break;
        }
        case kind::list:
            n->value = -1.0;
            n->widget.emplace<widgets::list_view>(widget_id.c_str());
            break;
        case kind::text:
        case kind::separator:
            break;
    }

    const int id = n->id;
    if(parent >= 0) {
        m_nodes[static_cast<std::size_t>(parent)]->children.push_back(id);
    }
    else {
        m_roots.push_back(id);
    }
    m_nodes.push_back(std::move(n));

    // Room for every node to report on one frame, so the frame itself never grows the vector.
    m_events.reserve(m_nodes.size());
    return id;
}

int form::size() const noexcept
{
    return static_cast<int>(m_nodes.size());
}

bool form::valid(int node) const noexcept
{
    return node >= 0 && node < static_cast<int>(m_nodes.size());
}

kind form::kind_of(int node) const noexcept
{
    return valid(node) ? m_nodes[static_cast<std::size_t>(node)]->what : kind::text;
}

void form::set_range(int node, double min_value, double max_value)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.what != kind::slider && n.what != kind::number) {
        return;
    }
    if(!std::isfinite(min_value) || !std::isfinite(max_value)) {
        return;
    }

    if(n.what == kind::number) {
        min_value = std::round(min_value);
        max_value = std::round(max_value);
    }

    n.min = min_value;
    n.max = (std::max)(min_value, max_value);

    if(n.what == kind::slider) {
        std::get<widgets::slider>(n.widget).set_range(static_cast<float>(n.min), static_cast<float>(n.max));
    }
    else {
        std::get<widgets::number_input>(n.widget).set_range(static_cast<int>(n.min), static_cast<int>(n.max));
    }

    set_value(node, n.value);
}

void form::set_options(int node, std::span<const std::string_view> labels)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.what != kind::choice) {
        return;
    }

    std::get<widgets::segmented>(n.widget).set_options(labels);
    n.count = static_cast<int>(labels.size());
    set_value(node, n.value);
}

void form::set_items(int node, std::span<const widgets::list_item_content> items)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.what != kind::list) {
        return;
    }

    std::get<widgets::list_view>(n.widget).set_items(items);
    n.count = static_cast<int>(items.size());
    set_value(node, n.value);
}

void form::set_height(int node, float height)
{
    if(!valid(node) || !std::isfinite(height)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.what == kind::list) {
        n.height = (std::max)(height, 1.f);
    }
}

void form::set_value(int node, double value)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(!has_value(n.what)) {
        return;
    }

    // A NaN would compare unequal to everything forever, and every clamp below would let it through.
    if(std::isnan(value)) {
        return;
    }

    // **The user wins the gesture** - in the model too, not only on screen. While a slider is dragged or a
    // picker is open the widget ignores its setter; if the model took the write anyway it would hold the
    // owner's number under the user's hand, and the commit on release would carry the owner's number, not
    // what the user let go of (the form-program-set stand scene, before this check). The model follows the
    // widget until the gesture ends - draw() keeps it in step.
    if((n.what == kind::slider && std::get<widgets::slider>(n.widget).active())
        || (n.what == kind::color && std::get<widgets::color_field>(n.widget).active())) {
        return;
    }

    switch(n.what) {
        case kind::toggle:
            value = value != 0.0 ? 1.0 : 0.0;
            break;
        case kind::slider:
            value = std::clamp(value, n.min, n.max);
            break;
        case kind::number:
            value = std::clamp(std::round(value), n.min, n.max);
            break;
        case kind::choice:
            value = n.count > 0 ? std::clamp(std::round(value), 0.0, static_cast<double>(n.count - 1)) : 0.0;
            break;
        case kind::color:
            value = std::clamp(std::floor(value), 0.0, static_cast<double>((std::numeric_limits<ImU32>::max)()));
            break;
        case kind::list:
            value = std::round(value);
            if(value < 0.0 || value >= static_cast<double>(n.count)) {
                value = -1.0;
            }
            break;
        default:
            break;
    }

    n.value = value;
}

double form::value(int node) const noexcept
{
    return valid(node) ? m_nodes[static_cast<std::size_t>(node)]->value : 0.0;
}

void form::set_label(int node, std::string_view label)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.label == label) {
        return;
    }
    n.label.assign(label);

    switch(n.what) {
        case kind::group:
            std::get<widgets::item_group>(n.widget).set_group_title(n.label);
            break;
        case kind::slider:
            std::get<widgets::slider>(n.widget).set_label(n.label);
            break;
        case kind::color:
            std::get<widgets::color_field>(n.widget).set_label(n.label);
            break;
        case kind::button:
            std::get<widgets::button>(n.widget).set_label(n.label);
            n.button_w = -1.f;
            break;
        default:
            break;
    }
}

void form::set_tooltip(int node, std::string_view tooltip)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    if(n.tooltip != tooltip) {
        n.tooltip.assign(tooltip);
    }
}

void form::set_enabled(int node, bool enabled)
{
    if(!valid(node)) {
        return;
    }

    form::node& n = *m_nodes[static_cast<std::size_t>(node)];
    n.enabled = enabled;

    // Every node is drawn under begin_disabled() while off (draw_children); the kinds with a switch of their
    // own get it as well, for what it does besides dimming.
    std::visit(
        [enabled](auto& w) {
            using widget = std::decay_t<decltype(w)>;
            if constexpr(!std::is_same_v<widget, std::monostate> && !std::is_same_v<widget, widgets::item_group>) {
                w.set_enabled(enabled);
            }
        },
        n.widget);
}

void form::set_visible(int node, bool visible)
{
    if(valid(node)) {
        m_nodes[static_cast<std::size_t>(node)]->visible = visible;
    }
}

ImRect form::last_rect(int node) const noexcept
{
    return valid(node) ? m_nodes[static_cast<std::size_t>(node)]->rect : ImRect {};
}

float form::min_width() const noexcept
{
    return m_measured_min;
}

void form::draw()
{
    m_events.clear();
    m_hover_claimed = false;
    for(const auto& n : m_nodes) {
        n->rect = ImRect {};
    }

    m_page_width = ImGui::GetContentRegionAvail().x;
    m_min_width = 0.f;

    ImGui::PushID(m_id.c_str());
    draw_children(m_roots);
    ImGui::PopID();

    m_measured_min = m_min_width;

    if(!m_hover_claimed) {
        m_hover_node = -1;
    }
}

std::span<const event> form::events() const noexcept
{
    return m_events;
}

void form::draw_children(const std::vector<int>& children)
{
    for(const int id : children) {
        node& n = *m_nodes[static_cast<std::size_t>(id)];
        if(!n.visible) {
            continue;
        }

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;

        // A node switched off is drawn under one disabled scope, label and all: a widget's own switch dims the
        // widget, but the label beside a toggle is the form's, drawn by label_row, and would stay lit (the
        // form-disabled stand scene). Inside the scope the widget does not dim a second time
        // (kit::fade_disabled); its own switch is still set, for what it does besides dimming - a slider's
        // edit field and a colour's picker close.
        if(!n.enabled) {
            begin_disabled();
        }
        draw_node(n);
        if(!n.enabled) {
            end_disabled();
        }

        // The row's rectangle: from where it started, across the width it had, down to the bottom of the last
        // thing it laid out. For its tooltip - a group's children ran first and had their chance first, so a
        // tooltip of a row inside a group wins over the group's own.
        const ImRect row { start, ImVec2 { start.x + width, ImGui::GetItemRectMax().y } };
        n.rect = row;

        // What this row cannot do without, plus what the frames around it took off the page - measured, as the
        // difference between the page and the width this row was given, rather than summed from paddings.
        if(const float need = need_width(n); need > 0.f) {
            m_min_width = (std::max)(m_min_width, need + (m_page_width - width));
        }

        if(!n.tooltip.empty() && !m_hover_claimed) {
            if(kit::hovering(row)) {
                m_hover_claimed = true;
                const double now = ImGui::GetTime();
                if(m_hover_node != n.id) {
                    m_hover_node = n.id;
                    m_hover_since = now;
                }
                else if(now - m_hover_since >= static_cast<double>(ImGui::GetStyle().HoverDelayNormal)) {
                    ImGui::SetTooltip("%s", n.tooltip.c_str());
                }
            }
        }
    }
}

void form::draw_node(node& n)
{
    switch(n.what) {
        case kind::group: {
            auto& w = std::get<widgets::item_group>(n.widget);
            w.begin();
            draw_children(n.children);
            w.end();
            break;
        }

        case kind::toggle: {
            auto& w = std::get<widgets::toggle>(n.widget);
            w.set_checked(n.value != 0.0);
            paint::label_row(n.label.c_str(), w.size());
            w.update();
            if(w.changed()) {
                n.value = w.checked() ? 1.0 : 0.0;
                push_event(n.id, n.value, true);
            }
            break;
        }

        case kind::slider: {
            auto& w = std::get<widgets::slider>(n.widget);
            w.set_value(static_cast<float>(n.value));
            w.update();
            if(w.changed() || w.committed() || w.active()) {
                n.value = static_cast<double>(w.value());
            }
            // A release that moved nothing still ends the gesture - an owner saving on commit needs to hear it
            // once even if the last frames of the drag changed nothing.
            if(w.changed() || w.committed()) {
                push_event(n.id, n.value, w.committed());
            }
            break;
        }

        case kind::number: {
            auto& w = std::get<widgets::number_input>(n.widget);
            w.set_value(static_cast<int>(n.value));
            paint::label_row(n.label.c_str(), ImVec2 { k_number_w, ImGui::GetFrameHeight() });
            w.update();
            if(w.changed()) {
                n.value = static_cast<double>(w.value());
                push_event(n.id, n.value, true);
            }
            break;
        }

        case kind::choice: {
            auto& w = std::get<widgets::segmented>(n.widget);
            w.set_selected(static_cast<int>(n.value));
            const ImVec2 size { w.width(), w.height() };
            if(size.x > ImGui::GetContentRegionAvail().x * k_choice_beside_share) {
                if(!n.label.empty()) {
                    paint::paragraph(n.label, theme::text_primary);
                }
            }
            else {
                paint::label_row(n.label.c_str(), size);
            }
            w.update();
            if(w.changed()) {
                n.value = static_cast<double>(w.selected());
                push_event(n.id, n.value, true);
            }
            break;
        }

        case kind::color: {
            auto& w = std::get<widgets::color_field>(n.widget);
            w.set_color(to_rgba(n.value));
            w.update();
            if(w.changed() || w.committed() || w.active()) {
                n.value = to_packed(w.color());
            }
            if(w.changed() || w.committed()) {
                push_event(n.id, n.value, w.committed());
            }
            break;
        }

        case kind::button: {
            auto& w = std::get<widgets::button>(n.widget);
            if(n.button_w < 0.f) {
                n.button_w = (std::max)(k_button_min_w, ImGui::CalcTextSize(n.label.c_str()).x + k_button_pad * 2.f);
                w.set_size(ImVec2 { n.button_w, k_button_h });
            }
            w.update();
            if(w.clicked()) {
                push_event(n.id, 0.0, true);
            }
            break;
        }

        case kind::text:
            paint::paragraph(n.label, theme::text_secondary);
            break;

        case kind::separator:
            ImGui::Separator();
            break;

        case kind::list: {
            auto& w = std::get<widgets::list_view>(n.widget);
            w.set_selected(static_cast<int>(n.value));
            w.set_size(ImVec2 { 0.f, n.height });
            w.update();
            if(w.selection_changed()) {
                n.value = static_cast<double>(w.selected_index());
                push_event(n.id, n.value, true);
            }
            break;
        }
    }
}

float form::need_width(const node& n) const
{
    // The widest of the controls that keep their size, and some room for the label beside them; a slider and a
    // list can be narrow and still work, a strip of chips cannot be narrower than its chips. Text wraps, a
    // group is the sum of what is in it - neither has a width of its own.
    constexpr float k_label_room = 48.f;
    constexpr float k_stretchy_min = 120.f;

    switch(n.what) {
        case kind::toggle:
            return std::get<widgets::toggle>(n.widget).size().x + k_label_room;
        case kind::number:
            return k_number_w + k_label_room;
        case kind::choice:
            return std::get<widgets::segmented>(n.widget).width();
        case kind::button:
            return n.button_w;
        case kind::slider:
        case kind::color:
        case kind::list:
            return k_stretchy_min;
        default:
            return 0.f;
    }
}

void form::push_event(int id, double value, bool committed)
{
    m_events.push_back(event { id, value, committed });
}

void begin_disabled()
{
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, theme::disabled_alpha);
    ImGui::BeginDisabled();
}

void end_disabled()
{
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
}
} // namespace tw::ui::form
