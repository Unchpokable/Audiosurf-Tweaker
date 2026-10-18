#include "pch.hxx"

#include "lua/lua_ui.hxx"

#include "engine/engine_state.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_host.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/kit/anim.hxx"
#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/toggle.hxx"

#include <imgui.h>

namespace
{
namespace paint = tw::ui::kit::paint;
namespace theme = tw::ui::theme;

using tw::ui::widgets::button;
using tw::ui::widgets::toggle;

int g_tab_handle = -1;

// One set of controls per script row, indexed by row. Kept alive across frames because each widget
// owns its own hover/press animation - rebuilding them per frame would freeze a thumb mid-slide.
std::vector<toggle> g_toggles;
std::vector<button> g_reload_buttons;
std::vector<button> g_resume_buttons;

// The row's own hover fade, beside its controls' - indexed the same way, for the same reason.
std::vector<tw::ui::kit::anim::channel> g_row_hover;

// Whether a row's diagnostics are unfolded. Per row, and not persisted: it is a way of looking, not a
// setting.
std::vector<char> g_expanded;

// How many rows the vectors above have. A rescan only ever appends scripts, so they only ever grow -
// and the rows already there keep their widgets, mid-animation or not.
std::size_t g_built_for = 0;

button g_rescan_button { "lua_scripts_rescan", ImVec2 { 72.f, 22.f } }; // k_button_w x k_control_h

// The row being dragged to a new place in the run order, by script id; -1 when nothing is. A drag
// only lives while the tab is drawn every frame: g_drag_frame is the last frame it was, and a drag the
// tab was not there to see the end of (the menu closed under it) is dropped, not applied.
int g_drag_id = -1;
int g_drag_frame = -1;

constexpr float k_grip_w = 14.f;
constexpr float k_row_pad = 10.f;
constexpr float k_toggle_w = 40.f;
constexpr float k_button_w = 72.f;
constexpr float k_control_h = 22.f;
constexpr float k_gap = 4.f;

// What a row calls a script's state. Derived every frame from the things that actually decide it -
// see script_state in lua_script.hxx for why it is never stored.
tw::lua::script_state state_of(const tw::lua::script& script, int waiting_hooks) noexcept
{
    if(script.missing) {
        return tw::lua::script_state::missing;
    }
    if(script.failed) {
        return tw::lua::script_state::failed;
    }
    if(!script.enabled) {
        return tw::lua::script_state::off;
    }
    if(script.health.suspended) {
        return tw::lua::script_state::suspended;
    }
    if(!tw::engine::state::ready() || waiting_hooks > 0 || tw::lua::diag::waiting(script.id)) {
        return tw::lua::script_state::waiting;
    }

    return tw::lua::script_state::running;
}

ImVec4 colour_of(tw::lua::script_state state) noexcept
{
    switch(state) {
    case tw::lua::script_state::running:
        return theme::accent_primary;
    case tw::lua::script_state::waiting:
        return theme::text_muted;
    case tw::lua::script_state::suspended:
        return theme::text_warning;
    case tw::lua::script_state::failed:
        return theme::text_error;
    case tw::lua::script_state::off:
    case tw::lua::script_state::missing:
        return theme::text_faint;
    }

    return theme::text_muted;
}

ImVec4 colour_of(tw::lua::diag::level severity) noexcept
{
    switch(severity) {
    case tw::lua::diag::level::pending:
    case tw::lua::diag::level::info:
        return theme::text_secondary;
    case tw::lua::diag::level::warn:
        return theme::text_warning;
    case tw::lua::diag::level::error:
    case tw::lua::diag::level::fatal:
        return theme::text_error;
    }

    return theme::text_secondary;
}

// A tinted pill (kit::paint::pill). Returns its width, so a row of them can be laid out left to right.
float draw_chip(ImDrawList* draw, ImVec2 at, const char* text, const ImVec4& tint)
{
    paint::pill(draw, at, text, tint);
    return paint::pill_size(text).x;
}

// One line of text, cut to `width` with an ellipsis rather than wrapped: row heights are decided before
// anything is drawn, so a line that wrapped would draw straight through the row below. Returns whether
// it was cut.
bool draw_line(ImDrawList* draw, ImVec2 at, float width, const ImVec4& colour, const std::string& text)
{
    return paint::add_text_ellipsis(draw, at, at.x + width, paint::to_u32(colour), text.c_str());
}

// The whole of a line draw_line() had to cut, wrapped at a readable width rather than one screen-wide
// strip (script-manager.md §8.13).
void full_text_tooltip(const std::string& text)
{
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.f);
    ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// The unfolded diagnostics: one line per record - level, where, text, and how often. The first
// occurrence in full, traceback and all, is in the tooltip, because that is what an author needs and
// what nobody else wants to scroll past.
void draw_diagnostics(std::span<const tw::lua::diag::entry> entries, ImVec2 at, float width)
{
    ImDrawList* const draw = ImGui::GetWindowDrawList();
    const float line = ImGui::GetTextLineHeight();

    for(std::size_t i = 0; i < entries.size(); ++i) {
        const tw::lua::diag::entry& record = entries[i];
        const ImVec2 row { at.x, at.y + static_cast<float>(i) * (line + 2.f) };

        const ImVec4 colour = colour_of(record.severity);
        const std::string head = std::format("{} {}", tw::lua::diag::name(record.severity), record.where);
        const float head_w = ImGui::CalcTextSize(head.c_str()).x + 8.f;

        draw->AddText(row, paint::to_u32(colour), head.c_str());

        std::string tail = record.text;
        if(record.count > 1) {
            tail += std::format("  x{}", record.count);
        }
        (void)draw_line(draw, ImVec2 { row.x + head_w, row.y }, std::max(0.f, width - head_w), theme::text_secondary, tail);

        if(tw::ui::kit::hovering(ImRect { row, ImVec2 { row.x + width, row.y + line } })) {
            ImGui::SetTooltip("%s\n\nseen %u time(s), first at frame %u, last at frame %u",
                record.detail.c_str(),
                record.count,
                record.first_frame,
                record.last_frame);
        }
    }
}

// The list is drawn by hand rather than through widgets::list_view.
//
// list_view exists to answer "which one of these did the user pick" - it owns a selection, and its
// row is a fixed icon/text/subtext arrangement. A script row has no selection at all and needs live
// controls inside it, so driving list_view here would mean fighting it for the row's interior and
// then ignoring the answer it produces. The row below is the same card as list_item's
// (kit::paint::row_background) without pretending to be a list.
void draw_row(tw::lua::script& script, std::size_t index, float width)
{
    ImDrawList* const draw = ImGui::GetWindowDrawList();
    const float line = ImGui::GetTextLineHeight();

    std::vector<std::string> waiting_groups;
    const int hooks = tw::lua::api::subscription_count(script.id);
    const int waiting = tw::lua::api::waiting_count(script.id, &waiting_groups);
    const tw::lua::script_state state = state_of(script, waiting);

    const std::span<const tw::lua::diag::entry> entries = tw::lua::diag::entries(script.id);
    const bool expanded = g_expanded[index] != 0 && !entries.empty();

    // The note under the title says the most important thing about the script: why it failed, why it
    // was suspended, or - when nothing is wrong - what it is.
    std::string note;
    ImVec4 note_colour = theme::text_secondary;
    if(script.missing) {
        note = "The file is gone from the Scripts folder. Rescan picks it up again if it comes back.";
        note_colour = theme::text_faint;
    }
    else if(script.failed) {
        note = script.error;
        note_colour = theme::text_error;
    }
    else if(script.health.suspended) {
        note = "Suspended: " + script.health.reason;
        note_colour = theme::text_warning;
    }
    else {
        note = script.description;
    }
    const bool has_note = !note.empty();

    // The chip line is a pill's height, which is taller than a line of text by its padding.
    const float chip_h = paint::pill_height();
    const float chip_extra = chip_h - line;

    const float diag_h = expanded ? static_cast<float>(entries.size()) * (line + 2.f) + k_gap : 0.f;
    const float height = k_row_pad * 2.f + line * (has_note ? 3.f : 2.f) + k_gap * (has_note ? 2.f : 1.f) + chip_extra + diag_h;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 p_min = origin;
    const ImVec2 p_max { origin.x + width, origin.y + height };

    // hovering(), not an item: the row carries its own toggle and buttons, and an item here would take
    // hover away from them. Unlike the bare rect test it replaces, it stays dark under a window lying on
    // top (the sky panel docks over the menu) and on a page fading out.
    tw::ui::kit::anim::channel& hover = g_row_hover[index];
    hover.set(tw::ui::kit::hovering(ImRect { p_min, p_max }));
    paint::row_background(draw, p_min, p_max, hover.step(), 0.f);

    // A script that failed or was stopped is called out in the border rather than only in the text:
    // the reason is one line down and easy to skim past, and "why is this one off again" is the
    // question the tab exists to answer.
    if(state == tw::lua::script_state::failed || state == tw::lua::script_state::suspended) {
        draw->AddRect(p_min, p_max, paint::to_u32(colour_of(state)), theme::radius::row);
    }

    if(g_drag_id == script.id) {
        draw->AddRect(p_min, p_max, paint::to_u32(theme::accent_primary), theme::radius::row, 0, 1.5f);
    }

    const float buttons = (script.enabled ? k_button_w + 8.f : 0.f) + (script.health.suspended ? k_button_w + 8.f : 0.f);
    // A missing script has nothing to switch: its row is a record, not a control.
    const float controls_w = script.missing ? 0.f : k_toggle_w + buttons + k_row_pad;
    const float text_w = std::max(60.f, width - controls_w - k_row_pad * 2.f - k_grip_w);
    const float full_w = width - k_row_pad * 2.f - k_grip_w;

    const float x = p_min.x + k_row_pad + k_grip_w;
    float y = p_min.y + k_row_pad;

    // The handle the row is dragged by, down its whole left edge (script-coexistence.md §3.3).
    // A missing script has one too: it keeps its place for when the file comes back, so it can be moved.
    {
        const ImVec2 grip_min { p_min.x + 2.f, p_min.y };
        const ImVec2 grip_size { k_row_pad + k_grip_w - 6.f, height };

        ImGui::SetCursorScreenPos(grip_min);
        ImGui::PushID(script.id);
        const tw::ui::kit::interaction grip =
            tw::ui::kit::interact(ImRect { grip_min, ImVec2 { grip_min.x + grip_size.x, grip_min.y + grip_size.y } }, ImGui::GetID("grip"));
        ImGui::PopID();

        const bool grip_hot = grip.hovered || grip.held;
        if(grip.held && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.f)) {
            g_drag_id = script.id;
        }
        if(grip.hovered && g_drag_id < 0) {
            ImGui::SetTooltip("Drag to change the order. Scripts run top to bottom.");
        }

        // Two columns of three dots, beside the title and the line under it.
        const ImU32 dots = paint::to_u32(grip_hot || g_drag_id == script.id ? theme::text_secondary : theme::text_faint);
        const float centre_x = grip_min.x + grip_size.x * 0.5f;
        const float centre_y = p_min.y + k_row_pad + line;
        for(int column = 0; column < 2; ++column) {
            for(int dot = 0; dot < 3; ++dot) {
                const ImVec2 at { centre_x + (static_cast<float>(column) - 0.5f) * 4.f, centre_y + static_cast<float>(dot - 1) * 4.f };
                draw->AddCircleFilled(at, 1.2f, dots);
            }
        }
    }

    // Title line: name, then version and author in muted type beside it. Everything after the name is
    // optional, because a script with no header annotations at all still has to list cleanly.
    draw->AddText(ImVec2 { x, y }, paint::to_u32(theme::text_primary), script.name.c_str());

    float meta_x = x + ImGui::CalcTextSize(script.name.c_str()).x + 8.f;
    if(!script.version.empty()) {
        const std::string version = "v" + script.version;
        draw->AddText(ImVec2 { meta_x, y }, paint::to_u32(theme::text_muted), version.c_str());
        meta_x += ImGui::CalcTextSize(version.c_str()).x + 8.f;
    }
    if(!script.author.empty()) {
        const std::string by = "by " + script.author;
        if(meta_x + ImGui::CalcTextSize(by.c_str()).x < x + text_w) {
            draw->AddText(ImVec2 { meta_x, y }, paint::to_u32(theme::text_faint), by.c_str());
        }
    }

    // A description longer than the row ends in an ellipsis and is shown whole on hover; so is a long
    // error or suspension reason.
    if(has_note) {
        y += line + k_gap;
        const ImVec2 note_at { x, y };
        const ImRect note_bb { note_at, ImVec2 { x + text_w, y + line } };
        if(draw_line(draw, note_at, text_w, note_colour, note) && tw::ui::kit::hovering(note_bb)) {
            full_text_tooltip(note);
        }
    }

    // Chips: state, then cost when it is worth mentioning, then hooks, then the diagnostics fold.
    y += line + k_gap;
    float chip_x = x;

    // What the script costs, per kind of callback. On the cost chip when there is one, and on the
    // state chip always - a cheap script's number is the one that says the guard itself is cheap.
    const auto cost_tooltip = [&script]() {
        std::string breakdown = std::format("{:.3f} ms per engine frame on average", script.health.total_ms);
        for(int kind = 0; kind < tw::lua::k_callback_count; ++kind) {
            const float ms = script.health.average_ms[static_cast<std::size_t>(kind)];
            if(ms >= 0.001f) {
                breakdown += std::format("\n  {}  {:.3f} ms", tw::lua::callback_name(static_cast<tw::lua::callback>(kind)), ms);
            }
        }
        if(script.health.errors > 0) {
            breakdown += std::format("\n{} error(s) since it was loaded", script.health.errors);
        }
        ImGui::SetTooltip("%s", breakdown.c_str());
    };

    const ImVec2 state_at { chip_x, y };
    const float state_w = draw_chip(draw, state_at, tw::lua::state_name(state), colour_of(state));
    if(script.enabled && tw::ui::kit::hovering(ImRect { state_at, ImVec2 { state_at.x + state_w, state_at.y + chip_h } })) {
        cost_tooltip();
    }
    chip_x += state_w + 6.f;

    if(state == tw::lua::script_state::running && script.health.total_ms >= tw::lua::sched::soft_budget_ms()) {
        const std::string cost = std::format("{:.1f} ms", script.health.total_ms);
        const ImVec2 chip_at { chip_x, y };
        const float chip_w = draw_chip(draw, chip_at, cost.c_str(), theme::text_warning);

        if(tw::ui::kit::hovering(ImRect { chip_at, ImVec2 { chip_at.x + chip_w, chip_at.y + chip_h } })) {
            cost_tooltip();
        }

        chip_x += chip_w + 6.f;
    }

    if(hooks > 0) {
        const std::string text = waiting > 0 ? std::format("{} hook{}, {} waiting", hooks, hooks == 1 ? "" : "s", waiting)
                                             : std::format("{} hook{}", hooks, hooks == 1 ? "" : "s");
        const ImVec2 at { chip_x, y + chip_extra * 0.5f };
        draw->AddText(at, paint::to_u32(theme::text_muted), text.c_str());

        const float text_x = ImGui::CalcTextSize(text.c_str()).x;
        if(waiting > 0 && tw::ui::kit::hovering(ImRect { at, ImVec2 { at.x + text_x, at.y + line } })) {
            std::string groups = "Waiting for:";
            for(const std::string& group : waiting_groups) {
                groups += "\n  " + group;
            }
            ImGui::SetTooltip("%s", groups.c_str());
        }

        chip_x += text_x + 12.f;
    }

    if(!entries.empty()) {
        const std::string label = expanded ? "hide messages"
                                           : std::format("{} message{}", entries.size(), entries.size() == 1 ? "" : "s");
        const ImVec2 at { chip_x, y + chip_extra * 0.5f };
        const ImVec2 size = ImGui::CalcTextSize(label.c_str());

        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(static_cast<int>(index));
        const tw::ui::kit::interaction link =
            tw::ui::kit::interact(ImRect { at, ImVec2 { at.x + size.x, at.y + size.y } }, ImGui::GetID("diag"));
        ImGui::PopID();
        if(link.clicked) {
            g_expanded[index] = expanded ? 0 : 1;
        }

        const bool link_hovered = link.hovered;
        draw->AddText(at, paint::to_u32(link_hovered ? theme::text_primary : theme::accent_primary), label.c_str());
    }

    if(expanded) {
        y += chip_h + k_gap;
        draw_diagnostics(entries, ImVec2 { x, y }, full_w);
    }

    // The controls sit on top of the hand-drawn row: ImGui widgets need a cursor, so it is parked
    // where each one belongs and the row's own height is claimed with a Dummy afterwards. Top-aligned
    // rather than centred, so they stay put when the diagnostics unfold.
    if(script.missing) {
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2 { width, height });
        return;
    }

    const float control_y = p_min.y + k_row_pad - 3.f;
    float control_x = p_max.x - k_row_pad - k_toggle_w;

    ImGui::SetCursorScreenPos(ImVec2 { control_x, control_y });
    g_toggles[index].set_checked(script.enabled);
    g_toggles[index].update();
    if(g_toggles[index].changed()) {
        // The toggle reports what the user asked for; the registry reports what actually happened,
        // and those differ when a script fails to load. set_checked() above re-syncs on the next
        // frame, so a failed enable visibly springs back rather than lying.
        (void)tw::lua::registry::set_enabled(script.id, g_toggles[index].checked());
    }

    if(script.enabled) {
        control_x -= 8.f + k_button_w;
        ImGui::SetCursorScreenPos(ImVec2 { control_x, control_y });
        g_reload_buttons[index].set_label("Reload");
        g_reload_buttons[index].update();
        if(g_reload_buttons[index].clicked()) {
            tw::lua::registry::reload(script.id);
        }
    }

    // Resume keeps everything the script built up - its handles, its state, its hooks - and simply
    // lets it run again with a clean budget. Reload is the other answer: start over from the file.
    if(script.health.suspended) {
        control_x -= 8.f + k_button_w;
        ImGui::SetCursorScreenPos(ImVec2 { control_x, control_y });
        g_resume_buttons[index].set_label("Resume");
        g_resume_buttons[index].update();
        if(g_resume_buttons[index].clicked()) {
            (void)tw::lua::sched::resume(script.id);
        }
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2 { width, height });
}

// The header line: what the scripting layer is doing right now, and - while it is not running
// anything - what it is waiting for.
//
// **This is the visible half of Ф2.** Before it, a script that could not find its channels because
// the game had not loaded them yet reported that as a string of failures in the notefeed, and a user
// had no way to tell that from a broken script. Now the layer waits quietly and says so here, in one
// line, with the number that moves while it waits (loaded groups) and the signal it is waiting on
// (the engine's start group).
void draw_status()
{
    namespace state = tw::engine::state;

    const state::phase phase = state::current();
    const int groups = state::group_count();

    switch(phase) {
    case state::phase::detached:
        ImGui::TextColored(theme::text_muted, "Waiting for the game's first frame.");
        break;

    case state::phase::booting:
        ImGui::TextColored(theme::text_muted, "The game is loading - %d group%s so far.", groups, groups == 1 ? "" : "s");
        break;

    case state::phase::starting:
        ImGui::TextColored(theme::text_muted, "The game has handed over - settling (%d groups).", groups);
        break;

    case state::phase::ready:
        ImGui::TextColored(theme::text_secondary, "Ready - %d channel group%s loaded.", groups, groups == 1 ? "" : "s");
        break;

    case state::phase::busy:
        ImGui::TextColored(theme::text_secondary, "Loading a run - %d group%s, scripts still running.", groups, groups == 1 ? "" : "s");
        break;
    }

    // Only while waiting, and only as a second line: once the game is up, naming the start group is
    // noise. While it is not, it is the difference between "stuck" and "the loader is still going".
    if(phase == state::phase::booting || phase == state::phase::starting) {
        if(const char* const group = state::start_group(); group != nullptr && group[0] != '\0') {
            ImGui::TextColored(theme::text_faint, "Start group: %s", group);
        }
    }

    // What the layer itself has to say - a refused write, a dispatcher that failed. Rare, and never
    // any one script's, so it sits up here rather than in a row.
    const std::span<const tw::lua::diag::entry> layer = tw::lua::diag::entries(-1);
    if(!layer.empty()) {
        ImGui::Dummy(ImVec2 { 0.f, 2.f });
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        draw_diagnostics(layer, at, width);
        ImGui::Dummy(ImVec2 { width, static_cast<float>(layer.size()) * (ImGui::GetTextLineHeight() + 2.f) });
    }

    ImGui::Dummy(ImVec2 { 0.f, 4.f });
}

// While a row is being dragged: which gap between rows the pointer is over - the number of rows whose
// middle is above it - shown as a line, and applied when the button comes up. `spans` are the rows' top
// and bottom in run order. A gap next to the dragged row is no move and gets no line.
void drop_target(std::span<const ImVec2> spans, float left, float width)
{
    const int from = tw::lua::registry::place_of(g_drag_id);
    if(from < 0) {
        g_drag_id = -1;
        return;
    }

    const float pointer = ImGui::GetIO().MousePos.y;
    int gap = 0;
    for(const ImVec2& span : spans) {
        if(pointer > (span.x + span.y) * 0.5f) {
            ++gap;
        }
    }

    const bool moves = gap != from && gap != from + 1;

    if(moves) {
        const std::size_t at = static_cast<std::size_t>(gap);
        const float y = at == 0              ? spans.front().x - 3.f
                        : at == spans.size() ? spans.back().y + 3.f
                                             : (spans[at - 1].y + spans[at].x) * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2 { left, y }, ImVec2 { left + width, y }, paint::to_u32(theme::accent_primary), 2.f);
    }

    if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return;
    }

    if(moves) {
        tw::lua::registry::move(g_drag_id, gap > from ? gap - 1 : gap);
    }
    g_drag_id = -1;
}

void draw_tab()
{
    if(!tw::lua::host::is_running()) {
        ImGui::TextColored(theme::text_muted, "The scripting VM is not running.");
        const std::string_view error = tw::lua::host::last_error();
        if(!error.empty()) {
            ImGui::TextColored(theme::text_error, "%.*s", static_cast<int>(error.size()), error.data());
        }
        return;
    }

    draw_status();

    // Before the count is read: a rescan can add rows, and this frame should already draw them.
    // Picks up scripts dropped into the folder or deleted from it while the game runs - and TweakerUI
    // asks for the same thing over IPC after it installs or removes one (lua_remote).
    const float rescan_y = ImGui::GetCursorScreenPos().y;
    g_rescan_button.set_label("Rescan");
    g_rescan_button.update();
    if(g_rescan_button.clicked()) {
        tw::lua::registry::rescan();
    }
    ImGui::SameLine(0.f, 10.f);
    ImGui::SetCursorScreenPos(
        ImVec2 { ImGui::GetCursorScreenPos().x, rescan_y + (k_control_h - ImGui::GetTextLineHeight()) * 0.5f });
    ImGui::TextColored(theme::text_faint, "Picks up scripts added to or removed from the Scripts folder.");
    ImGui::TextColored(theme::text_faint, "Scripts run top to bottom - drag a row by its handle to change the order.");
    ImGui::Dummy(ImVec2 { 0.f, 2.f });

    const int count = tw::lua::registry::count();
    if(count == 0) {
        ImGui::TextColored(theme::text_muted, "No scripts found.");
        ImGui::TextColored(theme::text_faint, "Drop .lua files into engine\\TweakerStuff\\Scripts in the game folder, then Rescan.");
        return;
    }

    for(std::size_t i = g_built_for; i < static_cast<std::size_t>(count); ++i) {
        g_toggles.emplace_back(("lua_script_toggle_" + std::to_string(i)).c_str());
        g_reload_buttons.emplace_back(("lua_script_reload_" + std::to_string(i)).c_str(), ImVec2 { k_button_w, k_control_h });
        g_resume_buttons.emplace_back(("lua_script_resume_" + std::to_string(i)).c_str(), ImVec2 { k_button_w, k_control_h });
        g_row_hover.emplace_back(tw::ui::theme::motion::hover);
        g_expanded.push_back(0);
    }
    g_built_for = std::max(g_built_for, static_cast<std::size_t>(count));

    const float width = ImGui::GetContentRegionAvail().x;

    int hooks = 0;
    int running = 0;

    // A drag the tab did not see every frame of - the menu was closed, another tab was open - ends here,
    // unapplied: its release happened somewhere this code was not looking.
    if(g_drag_id >= 0 && g_drag_frame != ImGui::GetFrameCount() - 1) {
        g_drag_id = -1;
    }
    g_drag_frame = ImGui::GetFrameCount();

    // In run order, top first. Widgets stay indexed by id - a row keeps its toggle mid-animation when it
    // moves. Each row's top and bottom are kept for the drop below.
    const std::span<const int> order = tw::lua::registry::order();
    const float left = ImGui::GetCursorScreenPos().x;
    std::vector<ImVec2> spans;
    spans.reserve(order.size());

    // The gap between the cards is the item spacing (theme::space::row_gap), as in list_view.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { ImGui::GetStyle().ItemSpacing.x, theme::space::row_gap });
    for(const int id : order) {
        tw::lua::script* const script = tw::lua::registry::find(id);
        if(script == nullptr) {
            continue;
        }

        draw_row(*script, static_cast<std::size_t>(id), width);
        spans.push_back(ImVec2 { ImGui::GetItemRectMin().y, ImGui::GetItemRectMax().y });

        if(script->enabled) {
            ++running;
            hooks += tw::lua::api::subscription_count(script->id);
        }
    }
    ImGui::PopStyleVar();

    if(g_drag_id >= 0 && !spans.empty()) {
        drop_target(spans, left, width);
    }

    ImGui::Dummy(ImVec2 { width, 4.f });

    // Footer: what the running scripts are actually doing to the game. Hook counts are the honest
    // measure of "is this costing me anything" - a disabled script is not merely idle here, it has no
    // hooks at all, and the number says so.
    ImGui::TextColored(theme::text_muted, "%d of %d running - %d channel hook%s", running, count, hooks, hooks == 1 ? "" : "s");

    // Two scripts on one channel is legal and handled - every subscriber runs, and any of them can
    // suppress the call - but it is worth saying out loud, because it is the one interaction between
    // independently-written scripts that has no other symptom until something looks wrong.
    if(const int shared = tw::lua::api::shared_channel_count(); shared > 0) {
        ImGui::TextColored(
            theme::text_warning, "%d channel%s shared by more than one script", shared, shared == 1 ? "" : "s");
    }
}
} // namespace

namespace tw::lua::ui
{
void initialize() noexcept
{
    g_tab_handle = tw::ui::plugins::interactive::menu::add_extra_tab("Scripts", &draw_tab);
    TW_LOG_INFO("lua_ui: Scripts tab registered as tab {}", g_tab_handle);
}
} // namespace tw::lua::ui
