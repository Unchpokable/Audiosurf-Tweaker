// The scenes of the snapshot stand - see scene.hxx and shots.hxx.
//
// Two kinds. Bench scenes put one widget (or a few variants of it) on a window painted with the
// overlay's own surface colour and walk it through its states with scripted input. Module scenes draw
// the real overlay - menu, Player tab, notefeed, pins - over the fake host data, and reach into it
// through window landmarks, since nothing inside a module can be marked from here.
//
// Widget state lives in function-level statics. That is safe only because every scene runs in a
// process of its own (shots.cxx, run_all).
#include "scene.hxx"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/form/form.hxx"
#include "ui/image/svg.hxx"
#include "ui/kit/anim.hxx"
#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/overlay_config.hxx"
#include "ui/overlay_state.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/qp/qp_catalog.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/color_field.hxx"
#include "ui/widgets/color_picker.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/number_input.hxx"
#include "ui/widgets/popup_menu.hxx"
#include "ui/widgets/segmented.hxx"
#include "ui/widgets/slider.hxx"
#include "ui/widgets/tab_view.hxx"
#include "ui/widgets/toggle.hxx"

namespace
{
using tw::smoke::scene;
using tw::smoke::scene_ctx;
using namespace tw::ui::widgets;

namespace theme = tw::ui::theme;
namespace paint = tw::ui::kit::paint;
namespace menu = tw::ui::plugins::interactive::menu;

constexpr ImVec2 k_bench_canvas { 480.f, 200.f };
constexpr ImVec2 k_module_canvas { 1280.f, 800.f };

// ---------------------------------------------------------------------------------------------
// Bench window and the shared click script.
// ---------------------------------------------------------------------------------------------
void begin_bench(scene_ctx& ctx)
{
    ImGui::SetNextWindowPos(ImVec2 { 0.f, 0.f });
    ImGui::SetNextWindowSize(ctx.canvas());
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::surface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 20.f, 20.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##bench",
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void end_bench()
{
    ImGui::End();
}

// Frame numbers of the click script. Mid-transition shots sit four frames (~67 ms) into a 150 ms
// tween; settled ones well past the longest animation in the set (200 ms).
constexpr int k_idle_at = 8;
constexpr int k_hover_from = 9;
constexpr int k_hover_mid_at = 14;
constexpr int k_hover_at = 35;
constexpr int k_press_from = 36;
constexpr int k_press_at = 50;
constexpr int k_release_from = 51;
constexpr int k_release_mid_at = 56;
constexpr int k_settled_at = 80;

// Idle, hover (mid and settled), press, release (mid and settled) at `target`, then finish.
void click_script(scene_ctx& ctx, ImVec2 target)
{
    switch(ctx.frame()) {
        case k_idle_at:
            ctx.capture("idle");
            break;
        case k_hover_from:
            ctx.mouse_move(target);
            break;
        case k_hover_mid_at:
            ctx.capture("hover-mid");
            break;
        case k_hover_at:
            ctx.capture("hover");
            break;
        case k_press_from:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case k_press_at:
            ctx.capture("press");
            break;
        case k_release_from:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case k_release_mid_at:
            ctx.capture("release-mid");
            break;
        case k_settled_at:
            ctx.capture("settled");
            ctx.finish();
            break;
        default:
            break;
    }
}

// A single frame of a still scene, once fonts and icons have been baked.
void still_script(scene_ctx& ctx)
{
    if(ctx.frame() == k_idle_at) {
        ctx.capture("");
        ctx.finish();
    }
}

// Left click at `target` on frame `from`, released two frames later.
void click_at(scene_ctx& ctx, int from, ImVec2 target, ImGuiMouseButton button = ImGuiMouseButton_Left)
{
    if(ctx.frame() == from) {
        ctx.mouse_move(target);
    }
    else if(ctx.frame() == from + 1) {
        ctx.mouse_button(button, true);
    }
    else if(ctx.frame() == from + 2) {
        ctx.mouse_button(button, false);
    }
}

// ---------------------------------------------------------------------------------------------
// button
// ---------------------------------------------------------------------------------------------
void draw_buttons(scene_ctx& ctx)
{
    static button labelled { "bench_button", { 160.f, 32.f } };
    static button icon_only { "bench_button_icon", { 36.f, 36.f } };
    static button icon_label { "bench_button_icon_label", { 150.f, 34.f } };
    static bool ready = false;
    if(!ready) {
        icon_only.set_icon("icons/play.svg");
        icon_label.set_icon("icons/music.svg");
        ready = true;
    }

    begin_bench(ctx);
    labelled.update("Apply");
    ctx.mark("labelled");
    ImGui::SameLine();
    icon_only.update();
    ctx.mark("icon");
    ImGui::SameLine();
    icon_label.update("With label");
    ctx.mark("icon_label");
    end_bench();
}

void button_frame(scene_ctx& ctx)
{
    draw_buttons(ctx);
    click_script(ctx, ctx.point("labelled"));
}

void button_icon_frame(scene_ctx& ctx)
{
    draw_buttons(ctx);
    click_script(ctx, ctx.point("icon"));
}

// ---------------------------------------------------------------------------------------------
// toggle
// ---------------------------------------------------------------------------------------------
void toggle_frame(scene_ctx& ctx)
{
    static toggle off { "bench_toggle_off" };
    static toggle on { "bench_toggle_on" };
    static bool ready = false;
    if(!ready) {
        // Set the way every caller sets it - once, from state the widget did not produce.
        on.set_checked(true);
        ready = true;
    }

    begin_bench(ctx);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Off");
    ImGui::SameLine(80.f);
    off.update();
    ctx.mark("off");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("On");
    ImGui::SameLine(80.f);
    on.update();
    end_bench();

    click_script(ctx, ctx.point("off"));
}

// ---------------------------------------------------------------------------------------------
// segmented
// ---------------------------------------------------------------------------------------------
void segmented_frame(scene_ctx& ctx)
{
    static segmented advance { "bench_advance", 28.f };
    static segmented modes { "bench_modes", 28.f };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_advance[] = { "Auto", "Manual" };
        static const std::string_view k_modes[] = { "Native", "67%", "50%", "33%" };
        advance.set_options(k_advance);
        modes.set_options(k_modes);
        modes.set_selected(2);
        ready = true;
    }

    begin_bench(ctx);
    advance.update();
    ctx.mark("advance");
    ImGui::Spacing();
    modes.update();
    end_bench();

    click_script(ctx, ctx.point("advance", ImVec2 { 0.75f, 0.5f }));
}

void segmented_icons_frame(scene_ctx& ctx)
{
    static segmented modes { "bench_icon_modes", 28.f };
    static bool ready = false;
    if(!ready) {
        std::vector<std::string_view> labels;
        std::vector<std::string_view> icons;
        for(const auto& mode : tw::ui::qp::all_playback_modes()) {
            labels.push_back(mode.display_name);
            icons.push_back(mode.icon_key);
        }
        modes.set_options(labels);
        modes.set_icons(icons);
        ready = true;
    }

    begin_bench(ctx);
    modes.update();
    ctx.mark("modes");
    end_bench();

    // The fourth of six equal chips.
    click_script(ctx, ctx.point("modes", ImVec2 { 3.5f / 6.f, 0.5f }));
}

// ---------------------------------------------------------------------------------------------
// slider - the three real ranges that broke the stock ImGui slider (see main.cxx's bench).
// ---------------------------------------------------------------------------------------------
void slider_frame(scene_ctx& ctx)
{
    static slider density { "bench_density" };
    static slider light_size { "bench_light_size" };
    static slider glow { "bench_glow" };
    static bool ready = false;
    if(!ready) {
        density.set_label("Star density");
        density.set_range(0.005f, 0.30f);
        density.set_value(0.07f);
        light_size.set_label("Light size");
        light_size.set_range(0.99f, 1.f);
        light_size.set_value(0.9998f);
        glow.set_label("Light glow");
        glow.set_range(1.f, 1000.f);
        glow.set_value(320.f);
        ready = true;
    }

    begin_bench(ctx);
    density.update();
    ctx.mark("density");
    light_size.update();
    glow.update();
    ctx.mark("glow");
    end_bench();

    // Hover and drag on the track lane of the last slider, then a double click on the first one's
    // label to open its edit field.
    const int f = ctx.frame();
    if(f == k_idle_at) {
        ctx.capture("idle");
    }
    else if(f == 9) {
        ctx.mouse_move(ctx.point("glow", ImVec2 { 0.3f, 0.8f }));
    }
    else if(f == 30) {
        ctx.capture("hover");
    }
    else if(f == 31) {
        ctx.mouse_button(ImGuiMouseButton_Left, true);
    }
    else if(f == 40) {
        ctx.capture("press");
    }
    else if(f == 41) {
        ctx.mouse_move(ctx.point("glow", ImVec2 { 0.7f, 0.8f }));
    }
    else if(f == 50) {
        ctx.capture("drag");
    }
    else if(f == 51) {
        ctx.mouse_button(ImGuiMouseButton_Left, false);
    }
    else if(f == 60) {
        ctx.capture("released");
    }
    else if(f == 61) {
        ctx.mouse_move(ctx.point("density", ImVec2 { 0.2f, 0.2f }));
    }
    else if(f >= 62 && f <= 65) {
        ctx.mouse_button(ImGuiMouseButton_Left, f % 2 == 0);
    }
    else if(f == 80) {
        ctx.capture("edit");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// number_input
// ---------------------------------------------------------------------------------------------
void number_input_frame(scene_ctx& ctx)
{
    static number_input match_size { "bench_match_size" };
    static bool ready = false;
    if(!ready) {
        match_size.set_range(0, 24);
        match_size.set_value(8);
        ready = true;
    }

    begin_bench(ctx);
    ImGui::TextUnformatted("Minimum match size");
    match_size.update();
    ctx.mark("field");
    end_bench();

    // The "+" stepper: the last frame-height square of the field.
    const ImRect field = ctx.rect("field");
    const float step = ImGui::GetFrameHeight();
    click_script(ctx, ImVec2 { field.Max.x - step * 0.5f, (field.Min.y + field.Max.y) * 0.5f });
}

// ---------------------------------------------------------------------------------------------
// list_item / list_view
// ---------------------------------------------------------------------------------------------
void list_item_frame(scene_ctx& ctx)
{
    static list_item plain { "bench_item_plain", { 360.f, 36.f } };
    static list_item two_line { "bench_item_two_line", { 360.f, 46.f } };
    static list_item with_icon { "bench_item_icon", { 360.f, 36.f } };

    begin_bench(ctx);
    plain.update({ .text = "Neon Pulse" });
    two_line.update({ .text = "A playlist whose name is far too long for the column it lives in", .subtext = "12 tracks" });
    ctx.mark("two_line");
    if(two_line.clicked()) {
        two_line.set_selected(!two_line.selected());
    }
    with_icon.update({ .text = "Mono Track", .icon = tw::ui::image::svg::get_resource("icons/music.svg").sz24 });
    end_bench();

    click_script(ctx, ctx.point("two_line"));
}

constexpr float k_list_pad = 6.f; // list_view's WindowPadding
constexpr float k_list_row_two_line = 46.f;
constexpr float k_list_gap = 4.f;

void list_view_frame(scene_ctx& ctx)
{
    static list_view playlists { "bench_list", { 360.f, 220.f } };
    static bool ready = false;
    if(!ready) {
        const std::array<list_item_content, 6> items {
            list_item_content { .text = "Mixtape", .subtext = "3 tracks" },
            list_item_content { .text = "Grind", .subtext = "250 tracks" },
            list_item_content { .text = "A playlist whose name is far too long for the column", .subtext = "1 track" },
            list_item_content { .text = "Freeride", .subtext = "0 tracks" },
            list_item_content { .text = "Очень длинное название плейлиста на кириллице", .subtext = "7 tracks" },
            list_item_content { .text = "Late night", .subtext = "42 tracks" },
        };
        playlists.set_items(items);
        playlists.set_selected(0);
        ready = true;
    }

    begin_bench(ctx);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    playlists.update();
    ctx.mark("list", ImRect { origin, ImVec2 { origin.x + 360.f, origin.y + 220.f } });
    end_bench();

    // The third row.
    const ImRect list = ctx.rect("list");
    const float row_y = list.Min.y + k_list_pad + 2.f * (k_list_row_two_line + k_list_gap) + k_list_row_two_line * 0.5f;
    click_script(ctx, ImVec2 { list.Min.x + 120.f, row_y });
}

// ---------------------------------------------------------------------------------------------
// tab_view
// ---------------------------------------------------------------------------------------------
constexpr float k_tab_nav_pad = 6.f;  // tab_view k_nav_pad
constexpr float k_tab_height = 36.f;  // tab_view k_tab_height
constexpr float k_tab_gap = 2.f;      // tab_view k_tab_gap

ImVec2 tab_center(const ImRect& view, int index)
{
    return ImVec2 { view.Min.x + k_tab_nav_pad + 30.f,
        view.Min.y + k_tab_nav_pad + static_cast<float>(index) * (k_tab_height + k_tab_gap) + k_tab_height * 0.5f };
}

void tab_view_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_tabs", { 440.f, 160.f } };
    static toggle option { "bench_tab_toggle" };
    static button action { "bench_tab_button", { 120.f, 28.f } };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "General", "Audio", "Advanced" };
        tabs.set_tabs(k_labels);
        ready = true;
    }

    begin_bench(ctx);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    tabs.begin();
    if(tabs.begin_view(0)) {
        ImGui::TextUnformatted("General settings page.");
        action.update("Rescan");
        tabs.end_view();
    }
    if(tabs.begin_view(1)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Audio option");
        ImGui::SameLine();
        option.update();
        tabs.end_view();
    }
    if(tabs.begin_view(2)) {
        ImGui::TextWrapped("Advanced page with a longer paragraph of text that has to wrap inside the content area.");
        tabs.end_view();
    }
    tabs.end();
    ctx.mark("tabs", ImRect { origin, ImVec2 { origin.x + 440.f, origin.y + 160.f } });
    end_bench();

    click_script(ctx, tab_center(ctx.rect("tabs"), 1));
}

// ---------------------------------------------------------------------------------------------
// item_group
// ---------------------------------------------------------------------------------------------
void item_group_frame(scene_ctx& ctx)
{
    static item_group titled { "bench_group_titled", "Notefeed" };
    static item_group untitled { "bench_group_untitled" };
    static toggle option { "bench_group_toggle" };

    begin_bench(ctx);
    titled.begin();
    ImGui::TextUnformatted("Corner");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Show offline pin");
    ImGui::SameLine();
    option.update();
    titled.end();

    ImGui::SameLine(0.f, 20.f);
    untitled.begin();
    ImGui::TextUnformatted("Untitled group");
    ImGui::TextUnformatted("Second line");
    untitled.end();
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// popup_menu
// ---------------------------------------------------------------------------------------------
void popup_menu_frame(scene_ctx& ctx)
{
    static button opener { "bench_popup_open", { 140.f, 30.f } };
    static popup_menu popup { "bench_popup", "Context" };
    static list_item copy { "bench_popup_copy", { 160.f, 28.f } };
    static list_item paste { "bench_popup_paste", { 160.f, 28.f } };

    begin_bench(ctx);
    opener.update("Open menu");
    ctx.mark("opener");
    if(opener.clicked()) {
        popup.open();
    }
    end_bench();

    if(popup.opened()) {
        popup.begin();
        copy.update({ .text = "Copy" });
        paste.update({ .text = "Paste" });
        if(copy.clicked() || paste.clicked()) {
            popup.close();
        }
        popup.end();
    }

    const ImRect window = ctx.window("##tw_popup_bench_popup");
    const float title_h = ImGui::GetFontSize() + 12.f; // popup_menu: text height + k_title_pad_y * 2
    const int f = ctx.frame();
    click_at(ctx, 9, ctx.point("opener"));
    if(f == k_idle_at) {
        ctx.capture("idle");
    }
    else if(f == 16) {
        ctx.capture("open-mid");
    }
    else if(f == 40) {
        ctx.capture("open");
    }
    else if(f == 41) {
        ctx.mouse_move(ImVec2 { window.Min.x + 40.f, window.Min.y + title_h + 8.f + 14.f });
    }
    else if(f == 60) {
        ctx.capture("item-hover");
    }
    click_at(ctx, 61, ImVec2 { ctx.canvas().x - 20.f, ctx.canvas().y - 20.f });
    if(f == 68) {
        ctx.capture("close-mid");
    }
    else if(f == 95) {
        ctx.capture("closed");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// color_field / color_picker
// ---------------------------------------------------------------------------------------------
void color_field_frame(scene_ctx& ctx)
{
    static color_field zenith { "bench_zenith" };
    static color_field veil { "bench_veil" };
    static bool ready = false;
    if(!ready) {
        zenith.set_label("Zenith");
        zenith.set_color(ImVec4 { 0.10f, 0.28f, 0.62f, 1.f });
        veil.set_label("Veil (half transparent)");
        veil.set_color(ImVec4 { 0.95f, 0.45f, 0.20f, 0.5f });
        ready = true;
    }

    // A fixed-width column, as in the sky panel: color_field fills the width it is given.
    begin_bench(ctx);
    ImGui::BeginChild("##fields", ImVec2 { 320.f, 80.f }, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    zenith.update();
    ctx.mark("zenith");
    veil.update();
    ImGui::EndChild();
    end_bench();

    // color_field claims its whole row with a Dummy; the swatch sits at the row's right end.
    const ImRect row = ctx.rect("zenith");
    const ImVec2 swatch { row.Max.x - row.GetHeight() * 0.5f, (row.Min.y + row.Max.y) * 0.5f };
    const int f = ctx.frame();
    if(f == k_idle_at) {
        ctx.capture("idle");
    }
    else if(f == k_hover_from) {
        ctx.mouse_move(swatch);
    }
    else if(f == k_hover_at) {
        ctx.capture("hover");
    }
    click_at(ctx, 36, swatch);
    if(f == 80) {
        ctx.capture("open");
        ctx.finish();
    }
}

void color_picker_frame(scene_ctx& ctx)
{
    static color_picker picker { "bench_picker", { 280.f, 0.f } };
    static ImVec4 colour { 0.2f, 0.83f, 0.75f, 0.8f };

    begin_bench(ctx);
    picker.update(colour);
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// kit::paint helpers: ellipsis, glows.
// ---------------------------------------------------------------------------------------------
void draw_helpers_frame(scene_ctx& ctx)
{
    begin_bench(ctx);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImU32 text = paint::to_u32(theme::text_primary);
    const ImU32 guide = paint::to_u32(theme::border_subtle);

    constexpr std::array<float, 3> k_widths { 70.f, 140.f, 260.f };
    for(std::size_t i = 0; i < k_widths.size(); ++i) {
        const float y = o.y + static_cast<float>(i) * 22.f;
        draw->AddLine(ImVec2 { o.x + k_widths[i], y }, ImVec2 { o.x + k_widths[i], y + 18.f }, guide);
        paint::add_text_ellipsis(draw, ImVec2 { o.x, y }, o.x + k_widths[i], text, "Очень длинное название, Latin tail");
    }

    const float gy = o.y + 80.f;
    for(int i = 0; i < 3; ++i) {
        const float strength = static_cast<float>(i) * 0.5f;
        paint::add_text_glow(draw, ImVec2 { o.x + static_cast<float>(i) * 90.f, gy }, "Glow", text, theme::accent_primary, strength);
    }

    paint::add_rect_glow(draw, ImVec2 { o.x + 290.f, o.y + 10.f }, ImVec2 { o.x + 380.f, o.y + 50.f }, 8.f, theme::accent_primary, 1.f);
    paint::add_circle_glow(draw, ImVec2 { o.x + 335.f, o.y + 110.f }, 14.f, theme::accent_primary, 1.f);
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// Behaviour scenes for В5: the gestures and states the review found wrong, each run through frame by frame.
// ---------------------------------------------------------------------------------------------

// slider: a press on the track puts the thumb under the pointer, a drag follows it, Escape mid-drag puts
// back the value from before the press (R3, §6 decision 4), a click on the label changes nothing. Shots:
// pressed, dragged, after Escape, released, after a label click.
void slider_escape_frame(scene_ctx& ctx)
{
    static slider density { "bench_escape_density" };
    static bool ready = false;
    if(!ready) {
        density.set_label("Star density");
        density.set_range(0.005f, 0.30f);
        density.set_value(0.07f);
        ready = true;
    }

    begin_bench(ctx);
    density.update();
    ctx.mark("density");
    end_bench();

    const int f = ctx.frame();
    if(f == k_hover_from) {
        ctx.mouse_move(ctx.point("density", ImVec2 { 0.2f, 0.8f }));
    }
    else if(f == 12) {
        ctx.mouse_button(ImGuiMouseButton_Left, true);
    }
    else if(f == 20) {
        ctx.capture("pressed");
        ctx.mouse_move(ctx.point("density", ImVec2 { 0.8f, 0.8f }));
    }
    else if(f == 30) {
        ctx.capture("dragged");
        ctx.key(ImGuiKey_Escape, true);
    }
    else if(f == 31) {
        ctx.key(ImGuiKey_Escape, false);
    }
    else if(f == 40) {
        ctx.capture("escaped");
        ctx.mouse_button(ImGuiMouseButton_Left, false);
    }
    else if(f == 60) {
        ctx.capture("released");
        ctx.mouse_move(ctx.point("density", ImVec2 { 0.5f, 0.2f }));
    }
    else if(f == 62) {
        ctx.mouse_button(ImGuiMouseButton_Left, true);
    }
    else if(f == 64) {
        ctx.mouse_button(ImGuiMouseButton_Left, false);
    }
    else if(f == 80) {
        ctx.capture("label-click");
        ctx.finish();
    }
}

// tab_view: back to the page being left, a third of the way into the transition (R4). The two shots either
// side of the second click must show the pages at the same opacity - no page popping back to full.
void tab_view_back_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_back_tabs", { 440.f, 160.f } };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "General", "Audio" };
        tabs.set_tabs(k_labels);
        ready = true;
    }

    begin_bench(ctx);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    tabs.begin();
    if(tabs.begin_view(0)) {
        ImGui::TextUnformatted("General settings page.");
        tabs.end_view();
    }
    if(tabs.begin_view(1)) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.f);
        ImGui::TextUnformatted("Audio page, lower down.");
        tabs.end_view();
    }
    tabs.end();
    ctx.mark("tabs", ImRect { origin, ImVec2 { origin.x + 440.f, origin.y + 160.f } });
    end_bench();

    const int f = ctx.frame();
    click_at(ctx, 9, tab_center(ctx.rect("tabs"), 1));
    click_at(ctx, 13, tab_center(ctx.rect("tabs"), 0));
    // Input queued on a frame takes effect on the next: the release on frame 15 is a click on frame 16.
    if(f == 15) {
        ctx.capture("before-back");
    }
    else if(f == 16) {
        ctx.capture("after-back");
    }
    else if(f == 50) {
        ctx.capture("settled");
        ctx.finish();
    }
}

// tab_view navigation that does not fit (R8): long labels cut with an ellipsis, more tabs than the
// height holds, then the wheel over the column.
void tab_view_overflow_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_overflow_tabs", { 300.f, 150.f } };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "Clouds and their drift", "Stars", "Aurora over the horizon", "Sun", "Moon", "Fog" };
        tabs.set_tabs(k_labels);
        ready = true;
    }

    begin_bench(ctx);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    tabs.begin();
    if(tabs.begin_view(0)) {
        ImGui::TextUnformatted("Page");
        tabs.end_view();
    }
    tabs.end();
    ctx.mark("tabs", ImRect { origin, ImVec2 { origin.x + 300.f, origin.y + 150.f } });
    end_bench();

    const int f = ctx.frame();
    if(f == k_idle_at) {
        ctx.capture("idle");
        ctx.mouse_move(tab_center(ctx.rect("tabs"), 1));
    }
    else if(f == 10) {
        ctx.mouse_wheel(-3.f);
    }
    else if(f == 30) {
        ctx.capture("scrolled");
        ctx.finish();
    }
}

// list_view: the items replaced while a row is selected and another hovered (R5). The shot right after
// must show both still lit, not fading in from nothing.
void list_view_refresh_frame(scene_ctx& ctx)
{
    static list_view playlists { "bench_refresh_list", { 360.f, 160.f } };
    static bool ready = false;
    if(!ready) {
        const std::vector<list_item_content> items { { .text = "Mixtape" }, { .text = "Grind" }, { .text = "Freeride" } };
        playlists.set_items(items);
        playlists.set_selected(0);
        ready = true;
    }

    if(ctx.frame() == 30) {
        const std::vector<list_item_content> items { { .text = "Mixtape (12)" }, { .text = "Grind" }, { .text = "Freeride" } };
        playlists.set_items(items);
    }

    begin_bench(ctx);
    playlists.update();
    ctx.mark("list");
    end_bench();

    const int f = ctx.frame();
    if(f == k_hover_from) {
        // The second row's middle: list padding 6, first row 36, gap 6, half a row.
        ctx.mouse_move(ctx.point("list", ImVec2 { 0.3f, (6.f + 36.f + 6.f + 18.f) / 160.f }));
    }
    else if(f == 29) {
        ctx.capture("before");
    }
    else if(f == 31) {
        ctx.capture("after-refresh");
        ctx.finish();
    }
}

// Disabled (R12) and the toggle's own response (R13): a button, a toggle and a strip with set_enabled(false)
// under the pointer, beside enabled ones; then the enabled toggle hovered and pressed.
void disabled_frame(scene_ctx& ctx)
{
    static button live { "bench_live_button", { 110.f, 30.f } };
    static button dead { "bench_dead_button", { 110.f, 30.f } };
    static toggle live_toggle { "bench_live_toggle" };
    static toggle dead_toggle { "bench_dead_toggle" };
    static segmented dead_strip { "bench_dead_strip", 28.f };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "Auto", "Manual" };
        dead_strip.set_options(k_labels);
        dead.set_enabled(false);
        dead_toggle.set_enabled(false);
        dead_toggle.set_checked(true);
        dead_strip.set_enabled(false);
        ready = true;
    }

    begin_bench(ctx);
    live.update("Apply");
    ImGui::SameLine();
    dead.update("Apply");
    ctx.mark("dead");
    live_toggle.update();
    ctx.mark("live_toggle");
    ImGui::SameLine(0.f, 20.f);
    dead_toggle.update();
    dead_strip.update();
    end_bench();

    const int f = ctx.frame();
    if(f == k_hover_from) {
        ctx.mouse_move(ctx.point("dead"));
    }
    else if(f == 30) {
        ctx.capture("dead-hovered");
        ctx.mouse_move(ctx.point("live_toggle"));
    }
    else if(f == 50) {
        ctx.capture("toggle-hovered");
        ctx.mouse_button(ImGuiMouseButton_Left, true);
    }
    else if(f == 60) {
        ctx.capture("toggle-pressed");
        ctx.mouse_button(ImGuiMouseButton_Left, false);
    }
    else if(f == 90) {
        ctx.capture("toggle-on");
        ctx.finish();
    }
}

// popup_menu opened from a button with open_at (under it), a title too long for it (R7), its chrome on
// the very first frame (R14), then Escape (R14).
void popup_anchored_frame(scene_ctx& ctx)
{
    static button opener { "bench_anchor_open", { 140.f, 30.f } };
    static popup_menu popup { "bench_anchor_popup", "Boards of Canada - Roygbiv (a very long track title indeed)" };
    static list_item copy { "bench_anchor_copy", { 160.f, 28.f } };

    begin_bench(ctx);
    ImGui::SetCursorPos(ImVec2 { 20.f, 120.f });
    opener.update("Character");
    const ImRect anchor { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
    ctx.mark("opener");
    if(opener.clicked()) {
        popup.open_at(anchor.Min, anchor.Max);
    }
    end_bench();

    if(popup.opened()) {
        popup.begin();
        copy.update({ .text = "Copy" });
        popup.end();
    }

    const int f = ctx.frame();
    click_at(ctx, 9, ctx.point("opener"));
    if(f == 13) {
        ctx.capture("first-frames");
    }
    else if(f == 40) {
        ctx.capture("open");
        ctx.key(ImGuiKey_Escape, true);
    }
    else if(f == 41) {
        ctx.key(ImGuiKey_Escape, false);
    }
    else if(f == 75) {
        ctx.capture("escaped");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// paint::icon. Top row: one icon at whole, quarter and half-pixel x - all three have to come out the
// same, crisp, because the recipe snaps to the pixel grid (review R10). Bottom row: the same button and
// icon chip strip at full style alpha and at 0.5, as on a tab page halfway through a crossfade - the icon
// has to fade with its label, not as alpha squared (review R2).
// ---------------------------------------------------------------------------------------------
void paint_icon_frame(scene_ctx& ctx)
{
    static button full { "bench_icon_alpha_full", { 150.f, 34.f } };
    static button half { "bench_icon_alpha_half", { 150.f, 34.f } };
    static bool ready = false;
    if(!ready) {
        full.set_icon("icons/music.svg");
        half.set_icon("icons/music.svg");
        ready = true;
    }

    begin_bench(ctx);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImTextureID play = tw::ui::image::svg::get_resource("icons/play.svg").sz16;
    float x = o.x;
    for(const float frac : { 0.f, 0.25f, 0.5f }) {
        paint::icon(draw, play, ImVec2 { x + frac, o.y + frac }, 16.f, theme::text_primary);
        x += 40.f;
    }

    ImGui::SetCursorScreenPos(ImVec2 { o.x, o.y + 40.f });
    full.update("Music");
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    half.update("Music");
    ImGui::PopStyleVar();
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// paint::row_background - the card every list row is (list_item, a track, a Scripts row): idle, hover
// halfway and full, selected, selected under the pointer. The Scripts tab is not on the stand, so its rows
// are seen here, through the recipe they are drawn with.
// ---------------------------------------------------------------------------------------------
void paint_rows_frame(scene_ctx& ctx)
{
    begin_bench(ctx);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    constexpr float k_h = 26.f;
    constexpr std::array<std::pair<float, float>, 5> k_states { { { 0.f, 0.f }, { 0.5f, 0.f }, { 1.f, 0.f }, { 0.f, 1.f }, { 1.f, 1.f } } };
    constexpr std::array<const char*, 5> k_names { "idle", "hover 0.5", "hover", "selected", "selected + hover" };

    for(std::size_t i = 0; i < k_states.size(); ++i) {
        const float y = o.y + static_cast<float>(i) * (k_h + theme::space::row_gap);
        paint::row_background(draw, ImVec2 { o.x, y }, ImVec2 { o.x + 300.f, y + k_h }, k_states[i].first, k_states[i].second);
        draw->AddText(ImVec2 { o.x + 10.f, y + (k_h - ImGui::GetFontSize()) * 0.5f }, paint::to_u32(theme::text_primary), k_names[i]);
    }
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// paint::pill in both colourings: neutral, as a track's tags, and tinted with each colour a Scripts row
// gives a script's state - the Scripts tab is not on the stand, so its chips are seen here.
// ---------------------------------------------------------------------------------------------
void paint_pills_frame(scene_ctx& ctx)
{
    begin_bench(ctx);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();

    float x = o.x;
    for(const char* tag : { "Ninja Mono", "[as-4lane]", "Invisible road" }) {
        paint::pill(draw, ImVec2 { x, o.y }, tag);
        x += paint::pill_size(tag).x + 6.f;
    }

    const std::array<std::pair<const char*, const ImVec4*>, 5> states { {
        { "running", &theme::accent_primary },
        { "waiting", &theme::text_muted },
        { "suspended", &theme::text_warning },
        { "failed", &theme::text_error },
        { "off", &theme::text_faint },
    } };
    x = o.x;
    for(const auto& [name, tint] : states) {
        paint::pill(draw, ImVec2 { x, o.y + 40.f }, name, *tint);
        x += paint::pill_size(name).x + 6.f;
    }
    end_bench();

    still_script(ctx);
}

// ---------------------------------------------------------------------------------------------
// paint::window_begin and its close button and resize grip - the sky parameter panel's chrome, which is
// not on the stand otherwise: a closable window with a title too long for its bar, the close button idle,
// halfway into its hover fade and hovered.
// ---------------------------------------------------------------------------------------------
void paint_window_frame(scene_ctx& ctx)
{
    static tw::ui::kit::anim::channel close_hover { theme::motion::hover };

    begin_bench(ctx);
    end_bench();

    ImGui::SetNextWindowPos(ImVec2 { 20.f, 20.f });
    ImGui::SetNextWindowSize(ImVec2 { 260.f, 150.f });
    const ImRect title = paint::window_begin("##bench_window", "Nebula drift, with a name far too long for it", true);
    const ImRect slot = paint::close_button_rect(title);
    const tw::ui::kit::interaction pointer = tw::ui::kit::interact(slot, ImGui::GetID("##close"));
    close_hover.set(pointer.hovered);
    paint::close_button(ImGui::GetWindowDrawList(), slot, close_hover.step());
    const ImVec2 max { ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y };
    paint::resize_grip(ImGui::GetWindowDrawList(), max);
    ctx.mark("close", slot);
    ImGui::End();

    switch(ctx.frame()) {
        case k_idle_at:
            ctx.capture("idle");
            break;
        case k_hover_from:
            ctx.mouse_move(ctx.point("close"));
            break;
        case k_hover_mid_at:
            ctx.capture("hover-mid");
            break;
        case k_hover_at:
            ctx.capture("hover");
            ctx.finish();
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// A glow at the edge of a tab_view page (review R1): a full-width slider at its minimum, hovered, puts the
// thumb's glow against the page's left edge - as on the sky parameter panel. The page clips at its own
// edge, so the glow has to fit into the page's padding rather than be cut off in a straight line.
// ---------------------------------------------------------------------------------------------
void glow_clip_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_glow_tabs", { 440.f, 120.f } };
    static slider edge { "bench_glow_edge" };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "Sky", "Other" };
        tabs.set_tabs(k_labels);
        edge.set_label("Star density");
        edge.set_range(0.f, 1.f);
        edge.set_value(0.f);
        ready = true;
    }

    begin_bench(ctx);
    tabs.begin();
    if(tabs.begin_view(0)) {
        edge.update();
        ctx.mark("edge");
        tabs.end_view();
    }
    tabs.end();
    end_bench();

    if(ctx.frame() == k_hover_from) {
        ctx.mouse_move(ctx.point("edge", ImVec2 { 0.5f, 0.8f }));
    }
    else if(ctx.frame() == k_hover_at) {
        ctx.capture("hover");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// A texture uploaded on demand after the font atlas has been updated - the notefeed toast icon case.
//
// ImGui's GL3 backend leaves GL_UNPACK_ROW_LENGTH at the atlas width after a partial atlas update (its
// restore is guarded by a status it has already overwritten), and an icon at a size outside the baked
// set is uploaded mid-frame by svg::at(). Unless gl_upload_texture sets its own unpack state, the
// driver reads the 19-pixel icon with the atlas's row pitch: garbage, or an access violation inside the
// driver. Text at a new size forces the atlas update first; the icons come three frames later.
// ---------------------------------------------------------------------------------------------
void late_icon_frame(scene_ctx& ctx)
{
    begin_bench(ctx);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();

    if(ctx.frame() >= 2) {
        draw->AddText(ImGui::GetFont(), 37.f, o, paint::to_u32(theme::text_primary), "Atlas 0123 ЖЩЮ");
    }

    if(ctx.frame() >= 5) {
        const auto icon = tw::ui::image::svg::get_resource("icons/play.svg");
        float x = o.x;
        for(const int px : { 19, 21, 27, 40 }) {
            const ImVec2 p_min { x, o.y + 60.f };
            paint::icon(draw, icon.at(px), p_min, static_cast<float>(px), theme::text_primary);
            x += static_cast<float>(px) + 16.f;
        }
    }
    end_bench();

    if(ctx.frame() == 12) {
        ctx.capture("");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// kit::interact / kit::hovering against what plain rect geometry got wrong (overlay-widgets-history.md, §1):
// a window lying on top, and a tab page on its way out. The rows print what hovering() said, so
// the shot reads without a diff.
// ---------------------------------------------------------------------------------------------
void draw_hover_row(scene_ctx& ctx, std::string_view tag, float width)
{
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImRect row { o, ImVec2 { o.x + width, o.y + 36.f } };
    ImGui::Dummy(row.GetSize());

    const bool lit = tw::ui::kit::hovering(row);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(row.Min, row.Max, paint::to_u32(lit ? theme::surface_row_hover : theme::surface_row), 6.f);
    draw->AddText(ImVec2 { o.x + 10.f, o.y + 9.f }, paint::to_u32(theme::text_primary), lit ? "hovering()" : "not hovering");
    ctx.mark(tag, row);
}

void interact_covered_frame(scene_ctx& ctx)
{
    static button under { "bench_covered_button", { 160.f, 32.f } };

    begin_bench(ctx);
    draw_hover_row(ctx, "row", 440.f);
    ImGui::Spacing();
    ImGui::SetCursorPosX(270.f);
    under.update(under.hovered() ? "hovered" : "not hovered");
    ctx.mark("button");
    end_bench();

    // Over the right half of both. Begun after the bench, so it is on top.
    ImGui::SetNextWindowPos(ImVec2 { 250.f, 0.f });
    ImGui::SetNextWindowSize(ImVec2 { 230.f, ctx.canvas().y });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4 { theme::surface_elevated.x, theme::surface_elevated.y, theme::surface_elevated.z, 0.85f });
    ImGui::Begin("##cover",
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::SetCursorPos(ImVec2 { 20.f, 150.f });
    ImGui::TextUnformatted("A window on top");
    ImGui::End();
    ImGui::PopStyleColor();

    const int f = ctx.frame();
    if(f == k_hover_from) {
        ctx.mouse_move(ctx.point("row", ImVec2 { 0.25f, 0.5f }));
    }
    else if(f == 20) {
        ctx.capture("row-open");
    }
    else if(f == 21) {
        ctx.mouse_move(ctx.point("row", ImVec2 { 0.85f, 0.5f }));
    }
    else if(f == 40) {
        ctx.capture("row-covered");
    }
    else if(f == 41) {
        ctx.mouse_move(ctx.point("button"));
    }
    else if(f == 60) {
        ctx.capture("button-covered");
        ctx.finish();
    }
}

void interact_fading_page_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_fading_tabs", { 440.f, 160.f } };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_labels[] = { "Rows", "Other" };
        tabs.set_tabs(k_labels);
        ready = true;
    }

    // Switched between frames, while the pointer rests on the row of the page being left.
    if(ctx.frame() == 26) {
        tabs.set_selected_tab(1);
    }

    begin_bench(ctx);
    tabs.begin();
    if(tabs.begin_view(0)) {
        draw_hover_row(ctx, "row", 300.f);
        tabs.end_view();
    }
    if(tabs.begin_view(1)) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 100.f);
        ImGui::TextUnformatted("The other page.");
        tabs.end_view();
    }
    tabs.end();
    end_bench();

    const int f = ctx.frame();
    if(f == k_hover_from) {
        ctx.mouse_move(ctx.point("row"));
    }
    else if(f == 25) {
        ctx.capture("steady");
    }
    else if(f == 29) {
        ctx.capture("fading");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// Module scenes.
// ---------------------------------------------------------------------------------------------
constexpr ImVec2 k_menu_pos { 60.f, 40.f };
constexpr ImVec2 k_menu_size { 720.f, 560.f };

void open_menu(int tab)
{
    tw::ui::overlay_config::set_menu_pos(k_menu_pos);
    tw::ui::overlay_config::set_menu_size(k_menu_size);
    menu::show_tab(tab);
}

void capture_settled(scene_ctx& ctx, int at = 40)
{
    if(ctx.frame() == at) {
        ctx.capture("");
        ctx.finish();
    }
}

void statics_setup(scene_ctx&)
{
    tw::ui::plugins::statics::notefeed::push("Skybox: Nebula", "icons/music.svg");
    tw::ui::plugins::statics::notefeed::push("Tweak enabled: Invisible road");
    tw::ui::plugins::statics::notefeed::push("A notification long enough that it has to be handled somehow by the feed");
}

void statics_frame(scene_ctx& ctx)
{
    tw::smoke::draw_overlay(ctx);
    if(ctx.frame() == 20) {
        ctx.capture("arriving");
    }
    capture_settled(ctx, 90);
}

void menu_skins_setup(scene_ctx&)
{
    open_menu(0);
}

void menu_tweaks_setup(scene_ctx&)
{
    open_menu(1);
}

void menu_player_setup(scene_ctx&)
{
    open_menu(2);
}

void menu_settings_setup(scene_ctx&)
{
    open_menu(3);
}

void menu_offline_setup(scene_ctx&)
{
    tw::ui::overlay_state::set_host_connected(false);
    open_menu(0);
}

void menu_frame(scene_ctx& ctx)
{
    tw::smoke::draw_overlay(ctx);
    capture_settled(ctx);
}

void menu_crossfade_frame(scene_ctx& ctx)
{
    // Switched between frames, the way a click on the nav column would be.
    if(ctx.frame() == 20) {
        menu::show_tab(1);
    }

    tw::smoke::draw_overlay(ctx);

    if(ctx.frame() == 25) {
        ctx.capture("mid");
    }
    capture_settled(ctx, 50);
}

// A point inside the first track row of the Player tab.
ImVec2 first_track(const scene_ctx& ctx)
{
    const ImRect tracks = ctx.window("##qp_tracks");
    return ImVec2 { tracks.Min.x + 120.f, tracks.Min.y + 16.f };
}

void player_rows_frame(scene_ctx& ctx)
{
    tw::smoke::draw_overlay(ctx);

    const int f = ctx.frame();
    if(f == 40) {
        ctx.mouse_move(first_track(ctx));
    }
    else if(f == 45) {
        ctx.capture("hover-mid");
    }
    else if(f == 70) {
        ctx.capture("hover");
    }
    click_at(ctx, 71, first_track(ctx));
    if(f == 78) {
        ctx.capture("select-mid");
    }
    else if(f == 100) {
        ctx.capture("selected");
        ctx.finish();
    }
}

// Nav tab `index` of the track context menu's tab_view, inside its popup.
ImVec2 context_tab(const scene_ctx& ctx, int index)
{
    const ImRect popup = ctx.window("##tw_popup_qp_track_ctx");
    const float title_h = ImGui::GetFontSize() + 12.f; // popup_menu title band
    const ImRect view { ImVec2 { popup.Min.x + 8.f, popup.Min.y + title_h + 8.f }, popup.Max };
    return tab_center(view, index);
}

void player_context_frame(scene_ctx& ctx)
{
    tw::smoke::draw_overlay(ctx);

    const int f = ctx.frame();
    click_at(ctx, 40, first_track(ctx), ImGuiMouseButton_Right);
    if(f == 47) {
        ctx.capture("open-mid");
    }
    else if(f == 75) {
        ctx.capture("character");
    }
    click_at(ctx, 76, context_tab(ctx, 1));
    if(f == 110) {
        ctx.capture("tags");
    }
    click_at(ctx, 111, context_tab(ctx, 2));
    if(f == 145) {
        ctx.capture("mods");
        ctx.finish();
    }
}

void player_character_frame(scene_ctx& ctx)
{
    tw::smoke::draw_overlay(ctx);

    // The full-width Character button sits under both columns of the tab.
    const ImRect playlists = ctx.window("##list");
    const ImRect tracks = ctx.window("##qp_tracks");
    const ImVec2 target { playlists.Min.x + 60.f, (std::max)(playlists.Max.y, tracks.Max.y) + ImGui::GetStyle().ItemSpacing.y + 14.f };

    click_at(ctx, 40, target);
    if(ctx.frame() == 47) {
        ctx.capture("open-mid");
    }
    else if(ctx.frame() == 80) {
        ctx.capture("open");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// ui/form - a page of controls described as data, as a script's tab will be (script-tabs.md §3.3).
// Each scene draws the form on a tab_view page, the way the menu will, and prints what the form reported
// under it: the events are the half of the form a picture cannot otherwise show.
// ---------------------------------------------------------------------------------------------
using ui_kind = tw::ui::form::kind;
using ui_form = tw::ui::form::form;

// What the form said, one line per event, oldest first - drawn under the page.
std::vector<std::string> g_form_log;

void log_form_events(const ui_form& f, std::span<const char* const> names)
{
    for(const tw::ui::form::event& e : f.events()) {
        const char* name = e.node >= 0 && e.node < static_cast<int>(names.size()) ? names[static_cast<std::size_t>(e.node)] : "?";
        char line[160];
        std::snprintf(line, sizeof(line), "%s = %.3f%s", name, e.value, e.committed ? "   committed" : "");
        g_form_log.emplace_back(line);
    }
}

void draw_form_log(std::size_t keep)
{
    ImGui::Spacing();
    if(g_form_log.empty()) {
        paint::paragraph("(no events)", theme::text_muted);
        return;
    }
    const std::size_t first = g_form_log.size() > keep ? g_form_log.size() - keep : 0;
    for(std::size_t i = first; i < g_form_log.size(); ++i) {
        paint::paragraph(g_form_log[i], theme::text_secondary);
    }
}

// The page a script's tab will be: one tab, the form inside its page.
void draw_form_page(tab_view& tabs, ui_form& f, ImVec2 size, bool page_disabled = false)
{
    tabs.set_size(size);
    tabs.begin();
    if(tabs.begin_view(0)) {
        if(page_disabled) {
            tw::ui::form::begin_disabled();
        }
        f.draw();
        if(page_disabled) {
            tw::ui::form::end_disabled();
        }
        tabs.end_view();
    }
    tabs.end();
}

ImVec2 form_point(const ui_form& f, int node, ImVec2 rel)
{
    const ImRect r = f.last_rect(node);
    return ImVec2 { r.Min.x + (r.Max.x - r.Min.x) * rel.x, r.Min.y + (r.Max.y - r.Min.y) * rel.y };
}

double packed(const ImVec4& rgba)
{
    return static_cast<double>(ImGui::ColorConvertFloat4ToU32(rgba));
}

// Every kind on one page, two sections; click the toggle, the choice and the button.
void form_controls_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_form_tabs" };
    static ui_form f { "bench_form" };
    static std::vector<const char*> names;
    static int show = -1, corner = -1, reset = -1;
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_tab[] = { "PuzzlePRO" };
        tabs.set_tabs(k_tab);

        const int display = f.add(ui_kind::group, -1, "Display");
        show = f.add(ui_kind::toggle, display, "Show HUD");
        f.set_value(show, 1.0);
        const int scale = f.add(ui_kind::slider, display, "Scale");
        f.set_range(scale, 0.5, 2.0);
        f.set_value(scale, 1.0);
        corner = f.add(ui_kind::choice, display, "Corner");
        static const std::string_view k_corners[] = { "Left", "Right" };
        f.set_options(corner, k_corners);
        const int accent = f.add(ui_kind::color, display, "Accent");
        f.set_value(accent, packed(theme::accent_primary));

        const int behaviour = f.add(ui_kind::group, -1, "Behaviour");
        const int rows = f.add(ui_kind::number, behaviour, "Rows");
        f.set_range(rows, 1.0, 10.0);
        f.set_value(rows, 5.0);
        f.add(ui_kind::text, behaviour, "Rows are counted from the top of the board. A long sentence wraps to the section.");
        const int preset = f.add(ui_kind::list, behaviour, "");
        const tw::ui::widgets::list_item_content k_items[] = { { .text = "Classic" }, { .text = "Wide" }, { .text = "Compact" } };
        f.set_items(preset, k_items);
        f.set_height(preset, 150.f);
        f.set_value(preset, 1.0);

        f.add(ui_kind::separator, -1, "");
        reset = f.add(ui_kind::button, -1, "Reset best");

        names = { "Display", "Show HUD", "Scale", "Corner", "Accent", "Behaviour", "Rows", "text", "Preset", "separator", "Reset best" };
        ready = true;
    }

    begin_bench(ctx);
    draw_form_page(tabs, f, ImVec2 { 440.f, 560.f });
    log_form_events(f, names);
    draw_form_log(4);
    end_bench();

    switch(ctx.frame()) {
        case 8:
            ctx.capture("idle");
            ctx.mouse_move(form_point(f, show, ImVec2 { 0.97f, 0.5f }));
            break;
        case 20:
            ctx.capture("toggle-hovered");
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 22:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 40:
            ctx.capture("toggle-off");
            ctx.mouse_move(form_point(f, corner, ImVec2 { 0.97f, 0.5f }));
            break;
        case 42:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 44:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 60:
            ctx.capture("choice-right");
            ctx.mouse_move(form_point(f, reset, ImVec2 { 0.05f, 0.5f }));
            break;
        case 62:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 64:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 80:
            ctx.capture("button-clicked");
            ctx.finish();
            break;
        default:
            break;
    }
}

// A slider gesture through the form: press, drag, release; again with Escape; then the number's + stepper.
void form_events_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_form_events_tabs" };
    static ui_form f { "bench_form_events" };
    static const char* const k_names[] = { "Scale", "Rows" };
    static int scale = -1, rows = -1;
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_tab[] = { "Events" };
        tabs.set_tabs(k_tab);
        scale = f.add(ui_kind::slider, -1, "Scale");
        f.set_value(scale, 0.2);
        rows = f.add(ui_kind::number, -1, "Rows");
        f.set_range(rows, 1.0, 10.0);
        f.set_value(rows, 5.0);
        ready = true;
    }

    begin_bench(ctx);
    draw_form_page(tabs, f, ImVec2 { 440.f, 140.f });
    log_form_events(f, k_names);
    draw_form_log(9);
    end_bench();

    switch(ctx.frame()) {
        case 8:
            ctx.mouse_move(form_point(f, scale, ImVec2 { 0.3f, 0.85f }));
            break;
        case 10:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 14:
            ctx.mouse_move(form_point(f, scale, ImVec2 { 0.6f, 0.85f }));
            break;
        case 18:
            ctx.capture("dragging");
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 24:
            ctx.capture("released");
            ctx.mouse_move(form_point(f, scale, ImVec2 { 0.8f, 0.85f }));
            break;
        case 26:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 30:
            ctx.key(ImGuiKey_Escape, true);
            break;
        case 31:
            ctx.key(ImGuiKey_Escape, false);
            break;
        case 34:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 40:
            ctx.capture("escaped");
            ctx.mouse_move(form_point(f, rows, ImVec2 { 0.99f, 0.5f }));
            break;
        case 42:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 44:
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 60:
            ctx.capture("stepped");
            ctx.finish();
            break;
        default:
            break;
    }
}

// The owner writes its value every frame; the user drags anyway and wins until release, then the owner's
// value lands. A toggle the owner switches on mid-scene reports nothing - a programmatic set is not an event.
void form_program_set_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_form_set_tabs" };
    static ui_form f { "bench_form_set" };
    static const char* const k_names[] = { "Level", "Armed" };
    static int level = -1, armed = -1;
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_tab[] = { "Owner" };
        tabs.set_tabs(k_tab);
        level = f.add(ui_kind::slider, -1, "Level");
        armed = f.add(ui_kind::toggle, -1, "Armed");
        ready = true;
    }

    const int frame = ctx.frame();
    if(frame >= 10) {
        f.set_value(level, 0.25); // every frame, as a script pushing its own idea of the value would
    }
    if(frame == 12) {
        f.set_value(armed, 1.0);
    }

    begin_bench(ctx);
    draw_form_page(tabs, f, ImVec2 { 440.f, 140.f });
    log_form_events(f, k_names);
    draw_form_log(3);
    end_bench();

    switch(frame) {
        case 9:
            ctx.capture("idle");
            break;
        case 16:
            ctx.capture("owner-set");
            ctx.mouse_move(form_point(f, level, ImVec2 { 0.8f, 0.85f }));
            break;
        case 20:
            ctx.mouse_button(ImGuiMouseButton_Left, true);
            break;
        case 30:
            ctx.capture("user-holding");
            ctx.mouse_button(ImGuiMouseButton_Left, false);
            break;
        case 45:
            ctx.capture("owner-again");
            ctx.finish();
            break;
        default:
            break;
    }
}

// Disabled, two ways that must look the same: every node switched off on its own (left), and the whole page
// under form::begin_disabled() as a suspended script's will be (right). Both dim once, to disabled_alpha.
// The left form also hides a toggle and a whole group, which take no room.
void build_disabled_form(ui_form& f, bool each)
{
    const int display = f.add(ui_kind::group, -1, "Display");
    const int show = f.add(ui_kind::toggle, display, "Show HUD");
    f.set_value(show, 1.0);
    const int hidden = f.add(ui_kind::toggle, display, "Hidden toggle");
    const int scale = f.add(ui_kind::slider, display, "Scale");
    f.set_value(scale, 0.6);
    const int corner = f.add(ui_kind::choice, display, "Corner");
    static const std::string_view k_corners[] = { "Left", "Right" };
    f.set_options(corner, k_corners);
    const int accent = f.add(ui_kind::color, display, "Accent");
    f.set_value(accent, packed(theme::accent_primary));
    const int rows = f.add(ui_kind::number, display, "Rows");
    f.set_value(rows, 5.0);
    const int note = f.add(ui_kind::text, -1, "A note under the section.");
    const int list = f.add(ui_kind::list, -1, "");
    const tw::ui::widgets::list_item_content k_items[] = { { .text = "Classic" }, { .text = "Wide" } };
    f.set_items(list, k_items);
    f.set_height(list, 106.f);
    f.set_value(list, 0.0);
    const int reset = f.add(ui_kind::button, -1, "Reset best");
    const int gone = f.add(ui_kind::group, -1, "Hidden group");
    f.add(ui_kind::toggle, gone, "Inside the hidden group");

    if(each) {
        for(const int node : { show, scale, corner, accent, rows, note, list, reset }) {
            f.set_enabled(node, false);
        }
        f.set_visible(hidden, false);
        f.set_visible(gone, false);
    }
    else {
        f.set_visible(hidden, false);
        f.set_visible(gone, false);
    }
}

void form_disabled_frame(scene_ctx& ctx)
{
    static tab_view left_tabs { "bench_form_dis_left" };
    static tab_view right_tabs { "bench_form_dis_right" };
    static ui_form each { "bench_form_dis_each" };
    static ui_form page { "bench_form_dis_page" };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_left[] = { "Each" };
        static const std::string_view k_right[] = { "Page" };
        left_tabs.set_tabs(k_left);
        right_tabs.set_tabs(k_right);
        build_disabled_form(each, true);
        build_disabled_form(page, false);
        ready = true;
    }

    begin_bench(ctx);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    draw_form_page(left_tabs, each, ImVec2 { 420.f, 520.f });
    ImGui::SetCursorScreenPos(ImVec2 { origin.x + 440.f, origin.y });
    draw_form_page(right_tabs, page, ImVec2 { 420.f, 520.f }, true);
    end_bench();

    switch(ctx.frame()) {
        case 8:
            ctx.capture("idle");
            ctx.mouse_move(form_point(each, 1, ImVec2 { 0.97f, 0.5f }));
            break;
        case 30:
            ctx.capture("hover-on-disabled");
            ctx.finish();
            break;
        default:
            break;
    }
}

// A page narrower than a strip of long chips: the choice goes under its label. Long labels cut, not wrap.
void form_narrow_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_form_narrow_tabs" };
    static ui_form f { "bench_form_narrow" };
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_tab[] = { "Narrow" };
        tabs.set_tabs(k_tab);
        const int where = f.add(ui_kind::choice, -1, "Where the panel sits");
        static const std::string_view k_options[] = { "Top left", "Top right", "Bottom" };
        f.set_options(where, k_options);
        const int side = f.add(ui_kind::choice, -1, "Side");
        static const std::string_view k_sides[] = { "L", "R" };
        f.set_options(side, k_sides);
        f.add(ui_kind::toggle, -1, "A toggle whose label is far longer than the page is wide");
        f.add(ui_kind::slider, -1, "A slider whose label is far longer than the page is wide");
        f.add(ui_kind::number, -1, "A number with a long label");
        f.add(ui_kind::button, -1, "A button that says a lot");
        ready = true;
    }

    begin_bench(ctx);
    draw_form_page(tabs, f, ImVec2 { 300.f, 300.f });
    // What the menu would hold its window at for this page (Т2): the chip strip plus the page's own inset.
    char line[64];
    std::snprintf(line, sizeof(line), "min_width() = %.0f", static_cast<double>(f.min_width()));
    ImGui::Spacing();
    paint::paragraph(line, theme::text_secondary);
    end_bench();

    if(ctx.frame() == 8) {
        ctx.capture("idle");
        ctx.finish();
    }
}

// A tooltip waits for the hover delay; one on a row inside a group wins over the group's own.
void form_tooltip_frame(scene_ctx& ctx)
{
    static tab_view tabs { "bench_form_tip_tabs" };
    static ui_form f { "bench_form_tip" };
    static int show = -1, display = -1;
    static bool ready = false;
    if(!ready) {
        static const std::string_view k_tab[] = { "Tips" };
        tabs.set_tabs(k_tab);
        display = f.add(ui_kind::group, -1, "Display");
        f.set_tooltip(display, "The group's own tooltip");
        show = f.add(ui_kind::toggle, display, "Show HUD");
        f.set_tooltip(show, "Hides the whole panel without unloading the script");
        f.add(ui_kind::toggle, display, "No tooltip here");
        ready = true;
    }

    begin_bench(ctx);
    draw_form_page(tabs, f, ImVec2 { 440.f, 150.f });
    end_bench();

    switch(ctx.frame()) {
        case 8:
            ctx.mouse_move(form_point(f, show, ImVec2 { 0.3f, 0.5f }));
            break;
        case 20:
            ctx.capture("waiting");
            break;
        case 45:
            ctx.capture("shown");
            ctx.mouse_move(form_point(f, show + 1, ImVec2 { 0.3f, 0.5f }));
            break;
        case 80:
            ctx.capture("group-tip");
            ctx.finish();
            break;
        default:
            break;
    }
}

// item_group sized by its content (the theme editor's masonry) and filling the width (a settings section),
// with a filling group nested in a filling group.
void item_group_fill_frame(scene_ctx& ctx)
{
    static item_group shrink { "bench_fill_shrink", "By content" };
    static item_group fill { "bench_fill_fill", "Across the page" };
    static item_group outer { "bench_fill_outer", "Section" };
    static item_group inner { "bench_fill_inner", "Sub-section" };
    static toggle a { "bench_fill_a" };
    static toggle b { "bench_fill_b" };
    static toggle c { "bench_fill_c" };
    static bool ready = false;
    if(!ready) {
        fill.set_fill_width(true);
        outer.set_fill_width(true);
        inner.set_fill_width(true);
        ready = true;
    }

    begin_bench(ctx);
    // Content that does not reach for the width (label_row would): the group is as wide as the toggle and its word.
    shrink.begin();
    a.update();
    ImGui::SameLine();
    ImGui::TextUnformatted("Option");
    shrink.end();

    fill.begin();
    paint::label_row("Option", b.size());
    b.update();
    fill.end();

    outer.begin();
    paint::paragraph("Text in the section.", theme::text_secondary);
    inner.begin();
    paint::label_row("Option", c.size());
    c.update();
    inner.end();
    outer.end();
    end_bench();

    if(ctx.frame() == 8) {
        ctx.capture("idle");
        ctx.finish();
    }
}

// ---------------------------------------------------------------------------------------------
// Dynamic menu tabs (menu::set_dynamic_tabs) - scripts' tabs, faked with forms. One storyboard: the tabs
// appear, one is added under the selection, the selected one is removed, the rest relabelled and dimmed with
// the same keys, the same list sent again every frame from inside a page (a reload from the Scripts tab),
// and a page with a strip of chips too wide for the window grows it on the way in.
// ---------------------------------------------------------------------------------------------
ui_form g_dyn_forms[] = { ui_form { "dyn_a" }, ui_form { "dyn_b" }, ui_form { "dyn_c" } };
bool g_dyn_resend_inside = false;

menu::dynamic_tab dyn_tab(int token, std::string_view label, bool enabled = true);

// The list as the scene currently wants it; re-sent from inside page A while g_dyn_resend_inside is on.
std::vector<menu::dynamic_tab> g_dyn_list;

float draw_dyn_tab(int token)
{
    ui_form& f = g_dyn_forms[token];
    f.draw();
    if(g_dyn_resend_inside && token == 0) {
        menu::set_dynamic_tabs(g_dyn_list);
    }
    return f.min_width();
}

constexpr std::string_view k_dyn_keys[] = { "puzzlepro_hud.lua#0", "tracker.lua#0", "wide.lua#0" };

menu::dynamic_tab dyn_tab(int token, std::string_view label, bool enabled)
{
    return menu::dynamic_tab { k_dyn_keys[token], label, &draw_dyn_tab, token, enabled };
}

void send_dyn(std::vector<menu::dynamic_tab> list)
{
    g_dyn_list = std::move(list);
    menu::set_dynamic_tabs(g_dyn_list);
}

void menu_dynamic_setup(scene_ctx&)
{
    {
        ui_form& f = g_dyn_forms[0];
        const int display = f.add(ui_kind::group, -1, "Display");
        const int show = f.add(ui_kind::toggle, display, "Show HUD");
        f.set_value(show, 1.0);
        const int scale = f.add(ui_kind::slider, display, "Scale");
        f.set_range(scale, 0.5, 2.0);
        f.set_value(scale, 1.0);
        const int corner = f.add(ui_kind::choice, display, "Corner");
        static const std::string_view k_corners[] = { "Left", "Right" };
        f.set_options(corner, k_corners);
    }
    {
        ui_form& f = g_dyn_forms[1];
        f.add(ui_kind::text, -1, "Tracker page: the tab the storyboard removes while it is selected.");
        f.add(ui_kind::toggle, -1, "Track misses");
    }
    {
        ui_form& f = g_dyn_forms[2];
        const int where = f.add(ui_kind::choice, -1, "Where the panel sits");
        static const std::string_view k_where[] = { "Top left corner", "Top right corner", "Bottom left corner", "Bottom right corner" };
        f.set_options(where, k_where);
    }

    tw::ui::overlay_config::set_menu_pos(k_menu_pos);
    tw::ui::overlay_config::set_menu_size(ImVec2 { 520.f, 420.f });
    send_dyn({ dyn_tab(0, "PuzzlePRO"), dyn_tab(1, "Tracker") });
    menu::show_tab(0);
}

void menu_dynamic_frame(scene_ctx& ctx)
{
    const int f = ctx.frame();

    // Between frames, as anything the scripts do happens outside the menu's update.
    if(f == 2) {
        menu::show_tab(4); // the first tab after the four built-in ones - smoke_test registers no extra tabs
    }
    else if(f == 41) {
        send_dyn({ dyn_tab(0, "PuzzlePRO"), dyn_tab(1, "Tracker"), dyn_tab(2, "Wide") });
    }
    else if(f == 61) {
        menu::show_tab(5);
    }
    else if(f == 101) {
        send_dyn({ dyn_tab(0, "PuzzlePRO"), dyn_tab(2, "Wide") });
    }
    else if(f == 121) {
        send_dyn({ dyn_tab(0, "PuzzlePRO HUD - Settings"), dyn_tab(2, "Wide", false) });
    }
    else if(f == 141) {
        g_dyn_resend_inside = true;
    }
    else if(f == 161) {
        g_dyn_resend_inside = false;
        menu::show_tab(5);
    }

    tw::smoke::draw_overlay(ctx);

    switch(f) {
        case 40:
            ctx.capture("shown");
            break;
        case 60:
            ctx.capture("added-under-selection");
            break;
        case 100:
            ctx.capture("tracker-selected");
            break;
        case 120:
            ctx.capture("selected-removed");
            break;
        case 140:
            ctx.capture("relabelled-dimmed");
            break;
        case 160:
            ctx.capture("same-list-resent");
            break;
        case 200:
            ctx.capture("wide-grown");
            ctx.finish();
            break;
        default:
            break;
    }
}

const scene k_scenes[] = {
    { "button", "labelled button through hover/press/release", k_bench_canvas, nullptr, &button_frame },
    { "button-icon", "icon-only button through hover/press/release", k_bench_canvas, nullptr, &button_icon_frame },
    { "toggle", "toggles; click turns the first one on", k_bench_canvas, nullptr, &toggle_frame },
    { "segmented", "text chips; click picks the second", k_bench_canvas, nullptr, &segmented_frame },
    { "segmented-icons", "Quick Player mode glyph chips", k_bench_canvas, nullptr, &segmented_icons_frame },
    { "slider", "three real sky ranges: hover, drag, double-click edit", k_bench_canvas, nullptr, &slider_frame },
    { "number-input", "clamped field, click on +", k_bench_canvas, nullptr, &number_input_frame },
    { "list-item", "plain, two-line with ellipsis, icon", k_bench_canvas, nullptr, &list_item_frame },
    { "list-view", "playlist-like list; click the third row", ImVec2 { 480.f, 260.f }, nullptr, &list_view_frame },
    { "tab-view", "three tabs; click the second (crossfade)", k_bench_canvas, nullptr, &tab_view_frame },
    { "item-group", "titled and untitled groups", k_bench_canvas, nullptr, &item_group_frame },
    { "popup-menu", "open, hover an item, dismiss outside", ImVec2 { 480.f, 260.f }, nullptr, &popup_menu_frame },
    { "color-field", "opaque and half-transparent swatches; open the picker", ImVec2 { 480.f, 460.f }, nullptr, &color_field_frame },
    { "color-picker", "the full picker", ImVec2 { 480.f, 320.f }, nullptr, &color_picker_frame },
    { "draw-helpers", "ellipsis at three widths, text/rect/circle glow", k_bench_canvas, nullptr, &draw_helpers_frame },
    { "slider-escape", "press jumps to the pointer, drag, Escape, release, label click", k_bench_canvas, nullptr, &slider_escape_frame },
    { "tab-view-back", "back to the page being left, mid-transition", k_bench_canvas, nullptr, &tab_view_back_frame },
    { "tab-view-overflow", "long labels and more tabs than fit; wheel over the column", k_bench_canvas, nullptr, &tab_view_overflow_frame },
    { "list-view-refresh", "items replaced under a selection and a hover", k_bench_canvas, nullptr, &list_view_refresh_frame },
    { "disabled", "disabled button, toggle, strip; toggle hover and press", k_bench_canvas, nullptr, &disabled_frame },
    { "form-controls", "ui/form: every kind on a script page; click toggle, choice, button", ImVec2 { 480.f, 720.f }, nullptr, &form_controls_frame },
    { "form-events", "ui/form: slider drag, Escape mid-drag, number stepper - and what the form reported", ImVec2 { 480.f, 400.f }, nullptr, &form_events_frame },
    { "form-program-set", "ui/form: owner writes every frame; the user wins the gesture", ImVec2 { 480.f, 260.f }, nullptr, &form_program_set_frame },
    { "form-disabled", "ui/form: each node off vs the page under begin_disabled; hidden nodes", ImVec2 { 900.f, 560.f }, nullptr, &form_disabled_frame },
    { "form-narrow", "ui/form: choice under its label on a narrow page; long labels", ImVec2 { 340.f, 370.f }, nullptr, &form_narrow_frame },
    { "form-tooltip", "ui/form: tooltip after the hover delay; row inside a group wins", ImVec2 { 480.f, 260.f }, nullptr, &form_tooltip_frame },
    { "item-group-fill", "item_group by content and across the page, nested", ImVec2 { 480.f, 300.f }, nullptr, &item_group_fill_frame },
    { "popup-anchored", "open_at under a button, long title, first frame, Escape", ImVec2 { 480.f, 260.f }, nullptr, &popup_anchored_frame },
    { "paint-icon", "icon on whole/quarter/half pixel; icon button at alpha 1 and 0.5", k_bench_canvas, nullptr, &paint_icon_frame },
    { "paint-rows", "row_background: idle, hover, selected", k_bench_canvas, nullptr, &paint_rows_frame },
    { "paint-pills", "pill: neutral (track tags) and tinted (script states)", k_bench_canvas, nullptr, &paint_pills_frame },
    { "paint-window", "window chrome: long title, close button idle/hovered, resize grip", k_bench_canvas, nullptr, &paint_window_frame },
    { "glow-clip", "slider glow at the edge of a tab_view page", k_bench_canvas, nullptr, &glow_clip_frame },
    { "late-icon", "icons uploaded on demand after a font atlas update", k_bench_canvas, nullptr, &late_icon_frame },
    { "interact-covered", "hover under a window lying on top: row and button", k_bench_canvas, nullptr, &interact_covered_frame },
    { "interact-fading-page", "hover on a tab page on its way out", k_bench_canvas, nullptr, &interact_fading_page_frame },
    { "overlay-statics", "notefeed with three toasts, pins, watermark", k_module_canvas, &statics_setup, &statics_frame },
    { "menu-skins", "menu, Skins tab", k_module_canvas, &menu_skins_setup, &menu_frame },
    { "menu-tweaks", "menu, Tweaks tab", k_module_canvas, &menu_tweaks_setup, &menu_frame },
    { "menu-player", "menu, Player tab", k_module_canvas, &menu_player_setup, &menu_frame },
    { "menu-settings", "menu, Settings tab", k_module_canvas, &menu_settings_setup, &menu_frame },
    { "menu-offline", "menu and pins with the host disconnected", k_module_canvas, &menu_offline_setup, &menu_frame },
    { "menu-crossfade", "Skins -> Tweaks, mid-transition and settled", k_module_canvas, &menu_skins_setup, &menu_crossfade_frame },
    { "menu-dynamic-tabs", "dynamic tabs: add under the selection, remove the selected, relabel, resend from a page, grow", k_module_canvas, &menu_dynamic_setup, &menu_dynamic_frame },
    { "player-rows", "track row hover and selection", k_module_canvas, &menu_player_setup, &player_rows_frame },
    { "player-context", "track context menu: Character, Tags, Mods", k_module_canvas, &menu_player_setup, &player_context_frame },
    { "player-character", "default character popup", k_module_canvas, &menu_player_setup, &player_character_frame },
};
} // namespace

namespace tw::smoke
{
scene_ctx::scene_ctx(ImVec2 canvas, const ui::overlay_state::cache& overlay, const ui::qp::state::cache& quick_player) noexcept
    : m_canvas(canvas), m_overlay(&overlay), m_quick_player(&quick_player)
{
}

int scene_ctx::frame() const noexcept
{
    return m_frame;
}

ImVec2 scene_ctx::canvas() const noexcept
{
    return m_canvas;
}

const ui::overlay_state::cache& scene_ctx::overlay() const noexcept
{
    return *m_overlay;
}

const ui::qp::state::cache& scene_ctx::quick_player() const noexcept
{
    return *m_quick_player;
}

void scene_ctx::mouse_move(ImVec2 pos)
{
    ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
}

void scene_ctx::mouse_move(std::string_view tag, ImVec2 rel)
{
    mouse_move(point(tag, rel));
}

void scene_ctx::mouse_button(ImGuiMouseButton button, bool down)
{
    ImGui::GetIO().AddMouseButtonEvent(button, down);
}

void scene_ctx::key(ImGuiKey key, bool down)
{
    ImGui::GetIO().AddKeyEvent(key, down);
}

void scene_ctx::mouse_wheel(float steps)
{
    ImGui::GetIO().AddMouseWheelEvent(0.f, steps);
}

void scene_ctx::mark(std::string_view tag)
{
    mark(tag, ImRect { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() });
}

void scene_ctx::mark(std::string_view tag, const ImRect& rect)
{
    for(auto& [name, value] : m_marks) {
        if(name == tag) {
            value = rect;
            return;
        }
    }
    m_marks.emplace_back(std::string { tag }, rect);
}

ImRect scene_ctx::rect(std::string_view tag) const
{
    for(const auto& [name, value] : m_marks) {
        if(name == tag) {
            return value;
        }
    }
    return ImRect {};
}

ImVec2 scene_ctx::point(std::string_view tag, ImVec2 rel) const
{
    const ImRect r = rect(tag);
    return ImVec2 { r.Min.x + (r.Max.x - r.Min.x) * rel.x, r.Min.y + (r.Max.y - r.Min.y) * rel.y };
}

ImRect scene_ctx::window(std::string_view name_part) const
{
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    for(const ImGuiWindow* candidate : g.Windows) {
        if(!(candidate->Active || candidate->WasActive) || candidate->Hidden) {
            continue;
        }
        if(std::string_view { candidate->Name }.find(name_part) == std::string_view::npos) {
            continue;
        }
        return ImRect { candidate->Pos, ImVec2 { candidate->Pos.x + candidate->Size.x, candidate->Pos.y + candidate->Size.y } };
    }
    return ImRect {};
}

void scene_ctx::capture(std::string_view shot)
{
    m_captures.emplace_back(shot);
}

void scene_ctx::finish() noexcept
{
    m_finished = true;
}

void scene_ctx::begin_frame(int frame) noexcept
{
    m_frame = frame;
}

std::vector<std::string> scene_ctx::take_captures()
{
    return std::exchange(m_captures, {});
}

bool scene_ctx::finished() const noexcept
{
    return m_finished;
}

std::span<const scene> all_scenes()
{
    return k_scenes;
}

void draw_overlay(scene_ctx& ctx)
{
    tw::ui::plugins::statics::watermark::update();
    tw::ui::plugins::statics::pins::update(ctx.overlay());
    tw::ui::plugins::statics::notefeed::update();
    tw::ui::plugins::interactive::menu::update(ctx.overlay(), ctx.quick_player());
}
} // namespace tw::smoke
