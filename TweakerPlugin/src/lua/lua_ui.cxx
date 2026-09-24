#include "pch.hxx"

#include "lua/lua_ui.hxx"

#include "engine/engine_state.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_host.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/plugins/interactive/menu.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/toggle.hxx"

#include <imgui.h>

namespace
{
namespace detail = tw::ui::widgets::detail;
namespace theme = tw::ui::theme;

using tw::ui::widgets::button;
using tw::ui::widgets::toggle;

int g_tab_handle = -1;

// One set of controls per script row, indexed by row. Kept alive across frames because each widget
// owns its own hover/press animation - rebuilding them per frame would freeze a thumb mid-slide.
std::vector<toggle> g_toggles;
std::vector<button> g_reload_buttons;
std::vector<button> g_resume_buttons;

// Whether a row's diagnostics are unfolded. Per row, and not persisted: it is a way of looking, not a
// setting.
std::vector<char> g_expanded;

int g_built_for = -1; // registry::count() the widget vectors were sized for

constexpr float k_row_pad = 10.f;
constexpr float k_rounding = 6.f;
constexpr float k_toggle_w = 40.f;
constexpr float k_button_w = 72.f;
constexpr float k_control_h = 22.f;
constexpr float k_gap = 4.f;

// What a row calls a script's state. Derived every frame from the things that actually decide it -
// see script_state in lua_script.hxx for why it is never stored.
tw::lua::script_state state_of(const tw::lua::script& script, int waiting_hooks) noexcept
{
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

// A small pill: tinted background, text in the tint. Returns its width, so a row of them can be laid
// out left to right.
float draw_chip(ImDrawList* draw, ImVec2 at, const char* text, const ImVec4& tint)
{
    const ImVec2 size = ImGui::CalcTextSize(text);
    const float width = size.x + 12.f;

    ImVec4 background = tint;
    background.w *= 0.18f;

    draw->AddRectFilled(at, ImVec2 { at.x + width, at.y + size.y + 2.f }, detail::to_u32(background), 4.f);
    draw->AddText(ImVec2 { at.x + 6.f, at.y + 1.f }, detail::to_u32(tint), text);

    return width;
}

// One line of text, clipped to `width` rather than wrapped: row heights are decided before anything is
// drawn, so a line that wrapped would draw straight through the row below.
void draw_clipped(ImDrawList* draw, ImVec2 at, float width, const ImVec4& colour, std::string_view text)
{
    const float line = ImGui::GetTextLineHeight();
    draw->PushClipRect(at, ImVec2 { at.x + width, at.y + line }, true);
    draw->AddText(at, detail::to_u32(colour), text.data(), text.data() + text.size());
    draw->PopClipRect();
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

        draw->AddText(row, detail::to_u32(colour), head.c_str());

        std::string tail = record.text;
        if(record.count > 1) {
            tail += std::format("  x{}", record.count);
        }
        draw_clipped(draw, ImVec2 { row.x + head_w, row.y }, std::max(0.f, width - head_w), theme::text_secondary, tail);

        if(ImGui::IsMouseHoveringRect(row, ImVec2 { row.x + width, row.y + line })) {
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
// then ignoring the answer it produces. The row below reuses list_item's *look* (the same surface,
// rounding and hover treatment) without pretending to be a list.
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
    if(script.failed) {
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

    const float diag_h = expanded ? static_cast<float>(entries.size()) * (line + 2.f) + k_gap : 0.f;
    const float height = k_row_pad * 2.f + line * (has_note ? 3.f : 2.f) + k_gap * (has_note ? 2.f : 1.f) + 2.f + diag_h;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 p_min = origin;
    const ImVec2 p_max { origin.x + width, origin.y + height };

    const bool hovered = ImGui::IsMouseHoveringRect(p_min, p_max);
    const ImVec4 surface = hovered ? theme::surface_row_hover : theme::surface_row;
    draw->AddRectFilled(p_min, p_max, detail::to_u32(surface), k_rounding);

    // A script that failed or was stopped is called out in the border rather than only in the text:
    // the reason is one line down and easy to skim past, and "why is this one off again" is the
    // question the tab exists to answer.
    if(state == tw::lua::script_state::failed || state == tw::lua::script_state::suspended) {
        draw->AddRect(p_min, p_max, detail::to_u32(colour_of(state)), k_rounding);
    }

    const float buttons = (script.enabled ? k_button_w + 8.f : 0.f) + (script.health.suspended ? k_button_w + 8.f : 0.f);
    const float controls_w = k_toggle_w + buttons + k_row_pad;
    const float text_w = std::max(60.f, width - controls_w - k_row_pad * 2.f);
    const float full_w = width - k_row_pad * 2.f;

    const float x = p_min.x + k_row_pad;
    float y = p_min.y + k_row_pad;

    // Title line: name, then version and author in muted type beside it. Everything after the name is
    // optional, because a script with no header annotations at all still has to list cleanly.
    draw->AddText(ImVec2 { x, y }, detail::to_u32(theme::text_primary), script.name.c_str());

    float meta_x = x + ImGui::CalcTextSize(script.name.c_str()).x + 8.f;
    if(!script.version.empty()) {
        const std::string version = "v" + script.version;
        draw->AddText(ImVec2 { meta_x, y }, detail::to_u32(theme::text_muted), version.c_str());
        meta_x += ImGui::CalcTextSize(version.c_str()).x + 8.f;
    }
    if(!script.author.empty()) {
        const std::string by = "by " + script.author;
        if(meta_x + ImGui::CalcTextSize(by.c_str()).x < x + text_w) {
            draw->AddText(ImVec2 { meta_x, y }, detail::to_u32(theme::text_faint), by.c_str());
        }
    }

    if(has_note) {
        y += line + k_gap;
        draw_clipped(draw, ImVec2 { x, y }, text_w, note_colour, note);
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
    if(script.enabled && ImGui::IsMouseHoveringRect(state_at, ImVec2 { state_at.x + state_w, state_at.y + line + 2.f })) {
        cost_tooltip();
    }
    chip_x += state_w + 6.f;

    if(state == tw::lua::script_state::running && script.health.total_ms >= tw::lua::sched::soft_budget_ms()) {
        const std::string cost = std::format("{:.1f} ms", script.health.total_ms);
        const ImVec2 chip_at { chip_x, y };
        const float chip_w = draw_chip(draw, chip_at, cost.c_str(), theme::text_warning);

        if(ImGui::IsMouseHoveringRect(chip_at, ImVec2 { chip_at.x + chip_w, chip_at.y + line + 2.f })) {
            cost_tooltip();
        }

        chip_x += chip_w + 6.f;
    }

    if(hooks > 0) {
        const std::string text = waiting > 0 ? std::format("{} hook{}, {} waiting", hooks, hooks == 1 ? "" : "s", waiting)
                                             : std::format("{} hook{}", hooks, hooks == 1 ? "" : "s");
        const ImVec2 at { chip_x, y + 1.f };
        draw->AddText(at, detail::to_u32(theme::text_muted), text.c_str());

        const float text_x = ImGui::CalcTextSize(text.c_str()).x;
        if(waiting > 0 && ImGui::IsMouseHoveringRect(at, ImVec2 { at.x + text_x, at.y + line })) {
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
        const ImVec2 at { chip_x, y + 1.f };
        const ImVec2 size = ImGui::CalcTextSize(label.c_str());

        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(static_cast<int>(index));
        if(ImGui::InvisibleButton("diag", size)) {
            g_expanded[index] = expanded ? 0 : 1;
        }
        ImGui::PopID();

        const bool link_hovered = ImGui::IsItemHovered();
        draw->AddText(at, detail::to_u32(link_hovered ? theme::text_primary : theme::accent_primary), label.c_str());
    }

    if(expanded) {
        y += line + 2.f + k_gap;
        draw_diagnostics(entries, ImVec2 { x, y }, full_w);
    }

    // The controls sit on top of the hand-drawn row: ImGui widgets need a cursor, so it is parked
    // where each one belongs and the row's own height is claimed with a Dummy afterwards. Top-aligned
    // rather than centred, so they stay put when the diagnostics unfold.
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

    const int count = tw::lua::registry::count();
    if(count == 0) {
        ImGui::TextColored(theme::text_muted, "No scripts found.");
        ImGui::TextColored(theme::text_faint, "Drop .lua files into engine\\TweakerStuff\\Scripts in the game folder.");
        return;
    }

    if(g_built_for != count) {
        g_toggles.clear();
        g_reload_buttons.clear();
        g_resume_buttons.clear();
        for(int i = 0; i < count; ++i) {
            g_toggles.emplace_back(("lua_script_toggle_" + std::to_string(i)).c_str());
            g_reload_buttons.emplace_back(("lua_script_reload_" + std::to_string(i)).c_str(), ImVec2 { k_button_w, k_control_h });
            g_resume_buttons.emplace_back(("lua_script_resume_" + std::to_string(i)).c_str(), ImVec2 { k_button_w, k_control_h });
        }
        g_expanded.assign(static_cast<std::size_t>(count), 0);
        g_built_for = count;
    }

    const float width = ImGui::GetContentRegionAvail().x;

    int hooks = 0;
    int running = 0;

    for(int i = 0; i < count; ++i) {
        tw::lua::script* const script = tw::lua::registry::find(i);
        if(script == nullptr) {
            continue;
        }

        draw_row(*script, static_cast<std::size_t>(i), width);
        ImGui::Dummy(ImVec2 { width, 6.f });

        if(script->enabled) {
            ++running;
            hooks += tw::lua::api::subscription_count(script->id);
        }
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
