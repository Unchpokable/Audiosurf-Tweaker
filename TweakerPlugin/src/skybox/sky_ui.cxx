#include "pch.hxx"

#include "skybox/sky_ui.hxx"

#include "plugin/diagnostics.hxx"

#include "skybox/sky_catalog.hxx"
#include "skybox/sky_panel.hxx"
#include "skybox/sky_probe.hxx"
#include "skybox/sky_program.hxx"
#include "skybox/sky_sprites.hxx"
#include "skybox/sky_timer.hxx"
#include "skybox/skybox.hxx"
#include "skybox/skybox_config.hxx"

#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/segmented.hxx"
#include "ui/widgets/slider.hxx"
#include "ui/widgets/toggle.hxx"

#include <imgui.h>

namespace
{
// Which menu tab this module was given. sky_panel docks beside the menu only while the Skybox page
// is the one on screen, and with more than one externally-registered tab that is no longer simply
// "the extra tab".
int g_tab_handle = -1;

using tw::ui::widgets::button;
using tw::ui::widgets::list_item_content;
using tw::ui::widgets::list_view;
using tw::ui::widgets::segmented;
using tw::ui::widgets::slider;
using tw::ui::widgets::toggle;

toggle g_enabled_toggle { "skybox_enabled" };
toggle g_markers_toggle { "skybox_probe_markers" };
list_view g_list { "skybox_list" };
button g_refresh_btn { "skybox_refresh", { 100.f, 28.f } };
button g_params_btn { "skybox_params_open", { 120.f, 28.f } };
segmented g_quality { "skybox_quality", 24.f };


// Percentages behind the four chips, in chip order. Native first because it is the default and the
// one nobody has to think about; the rest are the halving-ish steps that actually change the cost.
constexpr std::array<int, 4> k_quality_steps { 100, 67, 50, 33 };

bool g_widgets_ready = false;

// Catalog generation the list rows were built from, so the rows are rebuilt on a rescan and not on
// every frame the tab happens to be open.
unsigned int g_rows_generation = 0;
bool g_rows_built = false;

// The tab has to scan at least once before it can show anything, and doing it on the first draw
// rather than at initialize() keeps a directory walk off the plugin's startup path.
bool g_scanned = false;

const char* kind_label(tw::skybox::entry_kind kind) noexcept
{
    switch(kind) {
        case tw::skybox::entry_kind::program:
            return "shader";
        case tw::skybox::entry_kind::shader_file:
            return "shader (.hlsl)";
        case tw::skybox::entry_kind::packed:
            return "built-in";
        case tw::skybox::entry_kind::face_dir:
            return "six faces";
        case tw::skybox::entry_kind::package:
            return "sky package";
        default:
            return "file";
    }
}

void rebuild_rows()
{
    const auto entries = tw::skybox::catalog();

    std::vector<list_item_content> items;
    items.reserve(entries.size());

    for(const auto& entry : entries) {
        list_item_content item;
        item.text = entry.display_name;

        item.subtext = kind_label(entry.kind);
        if(entry.face_size > 0) {
            item.subtext += " - " + std::to_string(entry.face_size) + "px faces";
        }

        items.push_back(std::move(item));
    }

    g_list.set_items(items);
    g_list.set_selected(tw::skybox::selected_catalog_index());

    g_rows_generation = tw::skybox::catalog_generation();
    g_rows_built = true;
}

void apply_entry(const tw::skybox::catalog_entry& entry)
{
    switch(entry.kind) {
        case tw::skybox::entry_kind::program:
        case tw::skybox::entry_kind::shader_file:
        case tw::skybox::entry_kind::package:
            // All three are "a shader paints the sky", and the config stores the same thing for each:
            // a built-in id, a path to a .hlsl, or a path to a package directory.
            tw::skybox::select_program(entry.id);
            break;
        case tw::skybox::entry_kind::packed:
            tw::skybox::select_packed(entry.id);
            break;
        default:
            tw::skybox::select_file(entry.id);
            break;
    }

    tw::ui::plugins::statics::notefeed::push("Skybox: " + entry.display_name);
}

// Nearest chip to whatever the config holds, so a hand-edited percentage still lands somewhere
// sensible instead of resetting the strip to Native.
int quality_index(int percent) noexcept
{
    int best = 0;
    for(int i = 1; i < static_cast<int>(k_quality_steps.size()); ++i) {
        if(std::abs(k_quality_steps[i] - percent) < std::abs(k_quality_steps[best] - percent)) {
            best = i;
        }
    }

    return best;
}

// One label on the left, one toggle pushed to the right edge. Returns true on the frame the toggle
// flipped.
bool toggle_row(const char* label, toggle& widget)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 44.f);
    widget.update();

    return widget.changed();
}

// Measured on the GPU, so it means the same thing on a fast card as on a slow one - which watching
// the frame rate does not. Shown in the tab rather than logged because the log is compiled out of
// release builds, and this number is worth having on the machines that are not this one.
void draw_timing(const tw::skybox::status& status)
{
    if(status.draw_microseconds <= 0.f) {
        return;
    }

    ImGui::TextDisabled("Sky draw: %.0f us on the GPU (%.3f ms)", status.draw_microseconds, status.draw_microseconds / 1000.f);
}

// D3DBLEND as a name rather than the number GetRenderState hands back. Worth the table: the pair is
// only ever read to answer "what would a blended pass inherit if it set nothing", and "src 2,
// dest 4" does not answer that to anyone without the SDK header open.
const char* blend_name(unsigned long value)
{
    switch(value) {
        case D3DBLEND_ZERO: return "ZERO";
        case D3DBLEND_ONE: return "ONE";
        case D3DBLEND_SRCCOLOR: return "SRCCOLOR";
        case D3DBLEND_INVSRCCOLOR: return "INVSRCCOLOR";
        case D3DBLEND_SRCALPHA: return "SRCALPHA";
        case D3DBLEND_INVSRCALPHA: return "INVSRCALPHA";
        case D3DBLEND_DESTALPHA: return "DESTALPHA";
        case D3DBLEND_INVDESTALPHA: return "INVDESTALPHA";
        case D3DBLEND_DESTCOLOR: return "DESTCOLOR";
        case D3DBLEND_INVDESTCOLOR: return "INVDESTCOLOR";
        case D3DBLEND_SRCALPHASAT: return "SRCALPHASAT";
        default: return "?";
    }
}


// What the geometry layer is doing, and nothing to turn.
//
// It used to be eleven sliders here: count, size, clumps, spread, four material knobs and a light
// aimed by hand. All eleven have moved into the sky's own manifest, where they are declared by
// whoever made the sky and drawn by the parameter panel alongside every other knob it has - and the
// hand-aimed light is gone rather than moved, because a cloud is lit by the sky's lights now.
//
// What is left is the pair of claims the panel cannot make: whether the layer exists at all in this
// sky, and whether the device would build it.
void draw_geometry_layer()
{
    if(!tw::skybox::sprites::enabled()) {
        return;
    }

    if(!tw::skybox::sprites::ready()) {
        // "Enabled" and "drawing" are different claims, and a device that refused the shaders would
        // otherwise look exactly like a layer nobody switched on.
        ImGui::PushStyleColor(ImGuiCol_Text, tw::ui::theme::text_error);
        ImGui::TextUnformatted("The device would not create the sprite shaders - see the log.");
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextDisabled("Geometry layer: %d sprites.", tw::skybox::sprites::live_count());
}

// What the intercepted frame looks like from inside the game (see sky_probe). Folded away because
// none of it changes while a song plays and none of it is what somebody opens this tab for - but
// the draw count in particular reads on the timing line above, which only ever measures one draw.
void draw_frame_facts()
{
    const tw::skybox::probe::facts facts = tw::skybox::probe::current();
    if(!facts.captured) {
        return;
    }

    if(!ImGui::TreeNode("Intercepted frame")) {
        return;
    }

    if(facts.draws_peak > 1) {
        // The one number here that is a warning rather than a fact: the whole sky is redrawn on
        // every match, and the GPU timer above reports a single one of them.
        ImGui::PushStyleColor(ImGuiCol_Text, tw::ui::theme::text_warning);
        ImGui::Text("Sky draws: %d this frame, %d at peak - the timing above is for one of them", facts.draws_last_frame, facts.draws_peak);
        ImGui::PopStyleColor();
    }
    else {
        ImGui::TextDisabled("Sky draws: %d this frame, %d at peak", facts.draws_last_frame, facts.draws_peak);
    }

    ImGui::TextDisabled("Render target: %dx%d (%s)", facts.target_width, facts.target_height, facts.on_back_buffer ? "back buffer" : "off-screen");
    ImGui::TextDisabled("Viewport: %dx%d", facts.viewport_width, facts.viewport_height);
    ImGui::TextDisabled("Depth buffer: %s", facts.depth_bound ? "bound" : "none");
    ImGui::TextDisabled("Vertex processing: %s", facts.software_vertex_processing ? "software" : "hardware");
    ImGui::TextDisabled("Game's blend state: %s (src %s, dest %s)",
        facts.alpha_blend_enabled ? "on" : "off",
        blend_name(facts.src_blend),
        blend_name(facts.dest_blend));

    ImGui::TreePop();
}

// Compiler output for a .hlsl the user is editing. Errors matter more than anything else on this
// tab while they are happening, so they get a colour and the full text, wrapped.
void draw_diagnostics(const tw::skybox::status& status)
{
    if(status.program_diagnostics.empty()) {
        return;
    }

    const bool broken = status.program != nullptr && !status.program->usable();

    ImGui::PushStyleColor(ImGuiCol_Text, broken ? tw::ui::theme::text_error : tw::ui::theme::text_warning);
    ImGui::TextWrapped("%.*s", static_cast<int>(status.program_diagnostics.size()), status.program_diagnostics.data());
    ImGui::PopStyleColor();
}

void draw_status_line(const tw::skybox::status& status)
{
    if(!status.enabled) {
        ImGui::TextUnformatted("Disabled - the game draws its own sky sphere.");
        return;
    }

    if(status.program != nullptr) {
        // Deliberately ahead of every cube map state below: with a program running none of it
        // applies, because no image is loaded at any point.
        if(status.program_compiling) {
            // Said out loud because the game keeps its own sky while this runs, and a heavy shader
            // takes seconds - without this the wait looks like nothing happening.
            ImGui::Text("Compiling %.*s...", static_cast<int>(status.program_name.size()), status.program_name.data());
        }
        else if(!status.sky_detected) {
            ImGui::TextUnformatted("Waiting for the game to load its sky (start a song).");
        }
        else {
            ImGui::Text("Shader: %.*s", static_cast<int>(status.program_name.size()), status.program_name.data());
        }

        draw_timing(status);
        draw_geometry_layer();
        draw_frame_facts();
        draw_diagnostics(status);
        return;
    }

    if(status.load_failed) {
        ImGui::TextUnformatted("Load failed - see the log. The game keeps its own sky.");
        return;
    }

    if(!status.cubemap_ready) {
        // Two genuinely different waits, and saying which one it is saves the user from wondering
        // whether anything is broken: the cube map is only built when the game's sky sphere is
        // actually about to be drawn.
        ImGui::TextUnformatted(
            status.sky_detected ? "Ready - builds on the next sky draw." : "Waiting for the game to load its sky (start a song).");
        return;
    }

    if(status.requested_face_size > status.face_size) {
        ImGui::Text("Active: %dpx faces (asked for %d - not enough memory)", status.face_size, status.requested_face_size);
        return;
    }

    ImGui::Text("Active: %dpx faces", status.face_size);
}

void draw_tab()
{
    if(!g_widgets_ready) {
        const tw::skybox::status initial = tw::skybox::current_status();
        g_enabled_toggle.set_checked(tw::skybox::is_enabled());
        g_markers_toggle.set_checked(initial.probe_markers);

        constexpr std::array<std::string_view, 4> k_quality_labels { "Native", "67%", "50%", "33%" };
        g_quality.set_options(k_quality_labels);
        g_quality.set_selected(quality_index(initial.shader_quality));


        g_widgets_ready = true;
    }

    if(!g_scanned) {
        tw::skybox::refresh_catalog();
        g_scanned = true;
    }

    if(!g_rows_built || g_rows_generation != tw::skybox::catalog_generation()) {
        rebuild_rows();
    }

    const tw::skybox::status status = tw::skybox::current_status();

    if(toggle_row("Replace the sky sphere", g_enabled_toggle)) {
        tw::skybox::set_enabled(g_enabled_toggle.checked());
        tw::ui::plugins::statics::notefeed::push(g_enabled_toggle.checked() ? "Skybox on" : "Skybox off");
    }

    ImGui::Spacing();
    draw_status_line(status);
    ImGui::Spacing();

    // Only the probe program has anything to do with this, so it only appears when the probe is the
    // one selected. In the tab rather than only in the .cfg because the question it answers is
    // answered by looking at the screen and flipping it back and forth, which is a miserable loop if
    // every flip costs a restart.
    // Shader-only: a cube map is one texture lookup per pixel, and rendering it small to stretch it
    // big would cost picture for nothing.
    if(status.program != nullptr) {
        ImGui::TextUnformatted("Render resolution");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - g_quality.width());
        g_quality.update();

        if(g_quality.changed()) {
            const int selected = std::clamp(g_quality.selected(), 0, static_cast<int>(k_quality_steps.size()) - 1);
            tw::skybox::set_shader_quality(k_quality_steps[static_cast<std::size_t>(selected)]);
        }

        ImGui::Spacing();
    }

    if(status.program_has_markers) {
        if(toggle_row("Axis markers", g_markers_toggle)) {
            tw::skybox::set_probe_markers(g_markers_toggle.checked());
        }
        ImGui::TextDisabled("+X red, +Y green, +Z blue, ring on the horizon.");
        ImGui::Spacing();
    }

    const auto entries = tw::skybox::catalog();

    if(entries.empty()) {
        ImGui::TextUnformatted("No skyboxes found.");
    }
    else {
        // Unlike the Skins tab there is no Apply button. Installing a skin unpacks files into the
        // game folder, which is worth gating behind a second click; choosing a sky map only swaps a
        // texture this plugin owns, and is undone by clicking another row.
        // Room for the button row plus the folder line under it, both of which live below the list.
        const float list_h = (std::max)(80.f, ImGui::GetContentRegionAvail().y - 60.f);
        g_list.set_size(ImVec2 { 0.f, list_h });
        g_list.update();

        if(g_list.selection_changed()) {
            const int selected = g_list.selected_index();
            if(selected >= 0 && static_cast<std::size_t>(selected) < entries.size()) {
                apply_entry(entries[static_cast<std::size_t>(selected)]);
            }
        }
    }

    ImGui::Spacing();

    g_refresh_btn.update("Rescan");
    if(g_refresh_btn.clicked()) {
        tw::skybox::refresh_catalog();
        rebuild_rows();
    }

    if(status.program != nullptr) {
        if(status.program->params.empty()) {
            // Said rather than left blank. An absent button has three possible meanings - this
            // shader has no knobs, the annotation did not parse, or the plugin is older than the
            // feature - and no way to tell them apart from the outside.
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("no parameters declared");
        }
        else {
            ImGui::SameLine();
            g_params_btn.update(tw::skybox::panel::open_requested() ? "Hide parameters" : "Parameters");
            if(g_params_btn.clicked()) {
                tw::skybox::panel::toggle();
            }
        }
    }

    // Its own row rather than a fourth item on the SameLine chain above: this is a filesystem path,
    // and on any real installation it is longer than whatever space the buttons left over. Ellipsis
    // rather than clipping, so a path that does not fit still ends somewhere deliberate.
    const std::string& dir = tw::skybox::config::skybox_dir();
    const char* dir_text = dir.empty() ? "set skybox_dir= in the .cfg to add your own" : dir.c_str();

    const ImVec2 dir_pos = ImGui::GetCursorScreenPos();
    tw::ui::widgets::detail::add_text_ellipsis(ImGui::GetWindowDrawList(),
        dir_pos,
        dir_pos.x + ImGui::GetContentRegionAvail().x,
        tw::ui::widgets::detail::to_u32(tw::ui::theme::text_muted),
        dir_text);
    ImGui::Dummy(ImVec2 { ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() });
}
} // namespace

namespace tw::skybox::ui
{
int tab_handle() noexcept
{
    return g_tab_handle;
}

void initialize() noexcept
{
    g_tab_handle = tw::ui::plugins::interactive::menu::add_extra_tab("Skybox", &draw_tab);
    TW_LOG_INFO("sky_ui: Skybox tab registered as tab {}", g_tab_handle);
}

void update() noexcept
{
    // Hot reload. The notification is the whole point of doing this from the UI layer: a shader that
    // silently recompiled looks exactly like one that did not, and the loop this is meant to support
    // is "save the file, alt-tab, look".
    // Both of these follow the menu. The reload poll stats a file, and the timer issues GPU queries
    // whose result is only ever read off a label in this tab - neither is worth doing on the frames
    // where there is nowhere to put the answer.
    const bool menu_open = tw::ui::plugins::interactive::menu::is_visible();

    tw::skybox::timer::set_enabled(menu_open);

    // Unlike the two above, this one does not follow the menu: it is the frame boundary for a
    // counter the draw hooks increment whether anybody is watching or not, and skipping it would
    // make the count accumulate across frames instead of describing one.
    tw::skybox::probe::on_frame();

    switch(poll_reload(menu_open)) {
        case reload_outcome::started:
            tw::ui::plugins::statics::notefeed::push("Shader changed - compiling");
            break;
        case reload_outcome::reloaded:
            tw::ui::plugins::statics::notefeed::push("Shader reloaded");
            break;
        case reload_outcome::failed:
            tw::ui::plugins::statics::notefeed::push("Shader did not compile - see the Skybox tab");
            break;
        default:
            break;
    }

    int from = 0;
    int to = 0;
    if(consume_downscale_notice(from, to)) {
        tw::ui::plugins::statics::notefeed::push(
            "Skybox reduced to " + std::to_string(to) + "px faces (" + std::to_string(from) + " would not fit)");
    }
}

void draw_windows() noexcept
{
    tw::skybox::panel::draw(current_status());
}
} // namespace tw::skybox::ui
