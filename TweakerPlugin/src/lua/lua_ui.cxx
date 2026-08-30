#include "pch.hxx"

#include "lua/lua_ui.hxx"

#include "lua/lua_host.hxx"

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

// One toggle per script row, indexed by row. Kept alive across frames because the widget owns its
// own hover/press animation - rebuilding it per frame would freeze the thumb mid-slide.
std::vector<toggle> g_toggles;
std::vector<button> g_reload_buttons;

int g_built_for = -1; // script_count() the widget vectors were sized for

constexpr float k_row_pad = 10.f;
constexpr float k_rounding = 6.f;
constexpr float k_toggle_w = 40.f;
constexpr float k_reload_w = 72.f;

// The list is drawn by hand rather than through widgets::list_view.
//
// list_view exists to answer "which one of these did the user pick" - it owns a selection, and its
// row is a fixed icon/text/subtext arrangement. A script row has no selection at all and needs two
// live controls inside it, so driving list_view here would mean fighting it for the row's interior
// and then ignoring the answer it produces. The row below reuses list_item's *look* (the same
// surface, rounding and hover treatment) without pretending to be a list.
void draw_row(const tw::lua::host::script_info& info, std::size_t index, float width)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float line = ImGui::GetTextLineHeight();
    const bool has_note = !info.description.empty() || info.failed;
    const float height = k_row_pad * 2.f + line * (has_note ? 2.f : 1.f) + (has_note ? 4.f : 0.f);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 p_min = origin;
    const ImVec2 p_max { origin.x + width, origin.y + height };

    const bool hovered = ImGui::IsMouseHoveringRect(p_min, p_max);
    const ImVec4 surface = hovered ? theme::surface_row_hover : theme::surface_row;
    draw->AddRectFilled(p_min, p_max, detail::to_u32(surface), k_rounding);

    // A script that failed to load is called out in the border rather than only in the text: the
    // reason is one line down and easy to skim past, and "why is this one off again" is the question
    // the tab exists to answer.
    if(info.failed) {
        draw->AddRect(p_min, p_max, detail::to_u32(theme::text_error), k_rounding);
    }

    const float controls_w = k_toggle_w + (info.enabled ? k_reload_w + 8.f : 0.f) + k_row_pad;
    const float text_w = std::max(60.f, width - controls_w - k_row_pad * 2.f);

    float y = p_min.y + k_row_pad;

    // Title line: name, then version and author in muted type beside it. Everything after the name is
    // optional, because a script with no header annotations at all still has to list cleanly.
    draw->AddText(ImVec2 { p_min.x + k_row_pad, y }, detail::to_u32(theme::text_primary), info.name.c_str());

    float meta_x = p_min.x + k_row_pad + ImGui::CalcTextSize(info.name.c_str()).x + 8.f;
    if(!info.version.empty()) {
        const std::string version = "v" + info.version;
        draw->AddText(ImVec2 { meta_x, y }, detail::to_u32(theme::text_muted), version.c_str());
        meta_x += ImGui::CalcTextSize(version.c_str()).x + 8.f;
    }
    if(!info.author.empty()) {
        const std::string by = "by " + info.author;
        if(meta_x + ImGui::CalcTextSize(by.c_str()).x < p_min.x + k_row_pad + text_w) {
            draw->AddText(ImVec2 { meta_x, y }, detail::to_u32(theme::text_faint), by.c_str());
        }
    }

    if(has_note) {
        y += line + 4.f;

        const std::string& note = info.failed ? info.error : info.description;
        const ImU32 colour = detail::to_u32(info.failed ? theme::text_error : theme::text_secondary);

        // Clipped rather than wrapped: the row height is decided above, so a two-line description
        // would draw straight through the row below it.
        draw->PushClipRect(ImVec2 { p_min.x + k_row_pad, y }, ImVec2 { p_min.x + k_row_pad + text_w, y + line }, true);
        draw->AddText(ImVec2 { p_min.x + k_row_pad, y }, colour, note.c_str());
        draw->PopClipRect();
    }

    // The controls sit on top of the hand-drawn row: ImGui widgets need a cursor, so it is parked
    // where each one belongs and the row's own height is claimed with a Dummy afterwards.
    const float control_y = p_min.y + (height - 22.f) * 0.5f;

    if(info.enabled) {
        ImGui::SetCursorScreenPos(ImVec2 { p_max.x - k_row_pad - k_toggle_w - 8.f - k_reload_w, control_y });
        g_reload_buttons[index].set_label("Reload");
        g_reload_buttons[index].update();
        if(g_reload_buttons[index].clicked()) {
            tw::lua::host::reload_script(info.id);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2 { p_max.x - k_row_pad - k_toggle_w, control_y });
    g_toggles[index].set_checked(info.enabled);
    g_toggles[index].update();
    if(g_toggles[index].changed()) {
        // The toggle reports what the user asked for; the host reports what actually happened, and
        // those differ when a script fails to load. set_checked() above re-syncs on the next frame,
        // so a failed enable visibly springs back rather than lying.
        (void)tw::lua::host::set_script_enabled(info.id, g_toggles[index].checked());
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2 { width, height });
}

void draw_tab()
{
    const int count = tw::lua::host::script_count();

    if(!tw::lua::host::is_running()) {
        ImGui::TextColored(theme::text_muted, "The scripting VM is not running.");
        const std::string_view error = tw::lua::host::last_error();
        if(!error.empty()) {
            ImGui::TextColored(theme::text_error, "%.*s", static_cast<int>(error.size()), error.data());
        }
        return;
    }

    if(count == 0) {
        ImGui::TextColored(theme::text_muted, "No scripts found.");
        ImGui::TextColored(theme::text_faint, "Drop .lua files into the scripts/ folder next to TweakerPlugin.dll.");
        return;
    }

    if(g_built_for != count) {
        g_toggles.clear();
        g_reload_buttons.clear();
        for(int i = 0; i < count; ++i) {
            g_toggles.emplace_back(("lua_script_toggle_" + std::to_string(i)).c_str());
            g_reload_buttons.emplace_back(("lua_script_reload_" + std::to_string(i)).c_str(), ImVec2 { k_reload_w, 22.f });
        }
        g_built_for = count;
    }

    const float width = ImGui::GetContentRegionAvail().x;

    for(int i = 0; i < count; ++i) {
        const tw::lua::host::script_info* info = tw::lua::host::script_at(i);
        if(info == nullptr) {
            continue;
        }

        draw_row(*info, static_cast<std::size_t>(i), width);
        ImGui::Dummy(ImVec2 { width, 6.f });
    }

    ImGui::Dummy(ImVec2 { width, 4.f });

    // Footer: what the running scripts are actually doing to the game. Hook counts are the honest
    // measure of "is this costing me anything" - a disabled script is not merely idle here, it has no
    // hooks at all, and the number says so.
    int hooks = 0;
    int running = 0;
    for(int i = 0; i < count; ++i) {
        if(const tw::lua::host::script_info* info = tw::lua::host::script_at(i); info != nullptr && info->enabled) {
            ++running;
            hooks += info->hooks;
        }
    }

    ImGui::TextColored(theme::text_muted, "%d of %d running - %d channel hook%s", running, count, hooks, hooks == 1 ? "" : "s");

    // Two scripts on one channel is legal and handled - every subscriber runs, and any of them can
    // suppress the call - but it is worth saying out loud, because it is the one interaction between
    // independently-written scripts that has no other symptom until something looks wrong.
    if(const int shared = tw::lua::host::shared_channel_count(); shared > 0) {
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
