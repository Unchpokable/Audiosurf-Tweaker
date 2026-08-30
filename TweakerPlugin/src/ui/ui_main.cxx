#include "pch.hxx"

#include "ui/ui_main.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/dinput8_hooks.hxx"
#include "framework/imgui_backend.hxx"
#include "framework/wndproc_hub.hxx"

#include "ipc/overlay_ipc.hxx"

#include "lua/lua_host.hxx"

#include "plugin/diagnostics.hxx"

#include "skybox/sky_ui.hxx"

#include "ui/image/svg.hxx"
#include "ui/overlay_state.hxx"
#include "ui/pending_actions.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/qp/qp_pending.hxx"
#include "ui/qp/qp_state.hxx"
#include "ui/qp/qp_wire.hxx"

namespace
{
// (Re)targets the ImGui backend at whatever device d3d9_hooks just bound - fires from
// bind_device() for every new CreateDevice, the hk_end_scene late-load path, and any Reset() that
// changes the focus window without recreating the device. See
// tw::framework::d3d9::attach_device_bind_listener for the exact firing conditions.
void on_device_bound(IDirect3DDevice9* device, HWND hwnd)
{
    tw::framework::imgui_backend::initialize(device, hwnd);
}

// Fires just before a different device replaces the current one (and from tw::ui::shutdown) -
// tears down just the DX9/Win32 backends, keeping the ImGui context alive for whatever device
// binds next.
void on_device_unbound()
{
    tw::framework::imgui_backend::unbind();
}

// The overlay's toggle key. Lives here, on the wndproc hub, rather than as a GetAsyncKeyState poll
// inside the menu: a poll only runs on frames the overlay actually draws (so it misses presses
// whenever the game is rendering to an offscreen target, or paused, or minimized), it fires for
// key presses the game window never received at all, and it reads the global async key state -
// which is exactly what a low-level input hook (DirectInput's own, among others) can render blind.
// A WM_KEYDOWN subscriber has none of those failure modes: msg-specific subscribers run before the
// catch-all ImGui bridge, so this sees the key first no matter what the overlay is doing.
bool handle_toggle_key(HWND /*hwnd*/, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result)
{
    if(wparam != VK_INSERT) {
        return false;
    }

    // Bit 30 of lparam is the previous key state: set means this is an auto-repeat, which would
    // otherwise strobe the menu open/closed for as long as the key is held.
    constexpr LPARAM k_previous_key_state = LPARAM { 1 } << 30;
    if(msg == WM_KEYDOWN && (lparam & k_previous_key_state) == 0) {
        tw::ui::plugins::interactive::menu::toggle_visible();
        TW_LOG_INFO("menu: toggled {} by VK_INSERT", tw::ui::plugins::interactive::menu::is_visible() ? "open" : "closed");
    }

    // Both edges are swallowed, repeats included: forwarding only the up half would hand the game
    // an unpaired key-up and leave whatever it binds to Insert stuck down.
    out_result = 0;
    return true;
}
} // namespace

namespace tw::ui
{
void initialize() noexcept
{
    tw::framework::d3d9::attach_ui_plugin(draw_frame);
    tw::framework::d3d9::attach_device_reset_listener(
        &tw::framework::imgui_backend::on_lost_device, &tw::framework::imgui_backend::on_reset_device);
    tw::framework::d3d9::attach_device_bind_listener(&on_device_bound, &on_device_unbound);

    // Msg-specific subscribers run ahead of catch-all ones (see wndproc_hub.hxx), so the toggle key
    // is resolved before the ImGui bridge ever sees it - the menu can therefore always be closed
    // again, even from a state where ImGui would have swallowed the keystroke.
    tw::framework::wndproc::subscribe(WM_KEYDOWN, &handle_toggle_key);
    tw::framework::wndproc::subscribe(WM_KEYUP, &handle_toggle_key);
    tw::framework::wndproc::subscribe_all(&tw::framework::imgui_backend::wndproc_bridge);

    // Both input paths gate on the same predicate: window messages (ImGui) and the game's raw
    // DirectInput reads. Same function, so they can never disagree about whether the overlay is
    // currently eating input.
    tw::framework::imgui_backend::attach_input_gate(&tw::ui::plugins::interactive::menu::is_visible);
    tw::framework::dinput::attach_input_gate(&tw::ui::plugins::interactive::menu::is_visible);

    // Indexes the packed SVGs only - the actual rasterizing/uploading waits for a bound renderer and
    // happens in draw_frame() below.
    tw::ui::image::svg::initialize();

    tw::ui::plugins::statics::watermark::initialize();
    tw::ui::plugins::statics::pins::initialize();
    tw::ui::plugins::statics::notefeed::initialize();
    tw::ui::plugins::interactive::menu::initialize();

    tw::ui::pending_actions::set_send_backend(&tw::ipc::send_overlay_command);
    tw::ui::qp::set_send_backend(&tw::ipc::send_overlay_command);

    tw::ipc::initialize();
}

void shutdown() noexcept
{
    tw::framework::d3d9::detach_ui_plugin();
    tw::framework::d3d9::detach_device_reset_listener(
        &tw::framework::imgui_backend::on_lost_device, &tw::framework::imgui_backend::on_reset_device);
    tw::framework::d3d9::detach_device_bind_listener(&on_device_bound, &on_device_unbound);
    tw::framework::imgui_backend::detach_input_gate();
    tw::framework::dinput::detach_input_gate();

    tw::ui::plugins::interactive::menu::shutdown();
    tw::ui::plugins::statics::notefeed::shutdown();
    tw::ui::plugins::statics::pins::shutdown();
    tw::ui::plugins::statics::watermark::shutdown();

    // Before imgui_backend::shutdown(): the icon textures can only be released while the device
    // that created them is still bound.
    tw::ui::image::svg::shutdown();

    tw::framework::imgui_backend::shutdown();
    tw::ipc::shutdown();
}
} // namespace tw::ui

namespace
{
// Owned by the render thread only. Updated opportunistically once per frame - see
// tw::ui::overlay_state::refresh() for why this never blocks even under contention.
tw::ui::overlay_state::cache g_overlay_cache;

// Quick Player's own snapshot, kept separate rather than merged into the one above: a playlist of a
// thousand entries and a tweak toggle have nothing to do with each other, and sharing a generation
// counter would make every tweak flip re-copy the track list.
tw::ui::qp::state::cache g_qp_cache;

// Last values notefeed was already told about - separate from g_overlay_cache so the diff below
// only touches these (a handful of bytes + a short string) on the rare frames where refresh()
// actually reports a change, instead of copying the whole cache (including skin_names) every
// single frame regardless.
std::string g_notified_skin;
std::array<std::uint8_t, tw::ui::overlay_state::k_tweak_count> g_notified_tweaks {};
bool g_overlay_seeded = false;

// Host state changes are the only notefeed source for now (see notefeed.hxx) - no wire op exists
// yet for host-pushed events like "song started" (see overlay-protocol.md, "Чего пока нет").
void notify_overlay_state_changes()
{
    if(!g_overlay_seeded) {
        g_notified_skin = g_overlay_cache.current_skin_name;
        g_notified_tweaks = g_overlay_cache.tweak_enabled;
        g_overlay_seeded = true;
        return;
    }

    if(g_overlay_cache.current_skin_name != g_notified_skin && !g_overlay_cache.current_skin_name.empty()) {
        tw::ui::plugins::statics::notefeed::push(
            "Skin applied: " + g_overlay_cache.current_skin_name, tw::ui::overlay_state::skin_icon_key());
    }
    g_notified_skin = g_overlay_cache.current_skin_name;

    for(std::size_t i = 0; i < g_notified_tweaks.size(); ++i) {
        if(g_notified_tweaks[i] != g_overlay_cache.tweak_enabled[i]) {
            const auto id = tw::ui::overlay_state::all_tweak_ids()[i];
            const bool now = g_overlay_cache.tweak_enabled[i] != 0;
            std::string text(tw::ui::overlay_state::tweak_display_name(id));
            text += now ? " enabled" : " disabled";
            tw::ui::plugins::statics::notefeed::push(text, tw::ui::overlay_state::tweak_icon_key(id));
        }
    }
    g_notified_tweaks = g_overlay_cache.tweak_enabled;
}
} // namespace

namespace tw::ui
{
void draw_frame(IDirect3DDevice9* device)
{
    // d3d9_hooks.cxx only ever calls this with a device that has already gone through
    // bind_device() (either just now, on hk_end_scene's own late-load path, or earlier from
    // hk_create_device/hk_reset) - and bind_device() always fires the on_device_bound listener
    // (see ui_main::initialize()) before returning, which drives imgui_backend::initialize().
    // If that init failed (e.g. a missing embedded font resource), there's nothing to retry here.
    if(!tw::framework::imgui_backend::is_initialized()) {
        return;
    }

    // Bakes every packed SVG into textures the first frame after a device binds, and again after
    // each rebind invalidated them. One bool test on every other frame.
    tw::ui::image::svg::update();

    // Non-blocking: on contention (the IPC thread is mid-write) this just keeps last frame's
    // snapshot.
    if(tw::ui::overlay_state::refresh(g_overlay_cache)) {
        notify_overlay_state_changes();
    }

    // Same non-blocking contract, its own generation counter: a Quick Player push and a tweak push
    // are independent, and neither should make the other re-copy its state.
    (void)tw::ui::qp::state::refresh(g_qp_cache);

    tw::framework::imgui_backend::new_frame();

    // Ticks pending NOTIFY_TWEAK/NOTIFY_SKIN confirmations/timeouts every frame, independent of
    // menu visibility - a request must keep waiting even if the user closes the menu right after
    // clicking (see ui/pending_actions.hxx).
    tw::ui::pending_actions::update(g_overlay_cache);
    tw::ui::qp::pending::update(g_qp_cache);

    // Before notefeed::update(), so a toast raised this frame is drawn this frame rather than
    // waiting for the next one.
    tw::skybox::ui::update();

    tw::ui::plugins::statics::watermark::update();
    tw::ui::plugins::statics::pins::update(g_overlay_cache);
    tw::ui::plugins::statics::notefeed::update();
    tw::ui::plugins::interactive::menu::update(g_overlay_cache, g_qp_cache);

    // Scripts draw last, after every one of the overlay's own widgets has decided where it is.
    //
    // This is deliberate and it is the whole reason tw.hud.rect() can be trusted: each widget's
    // rectangle depends on text it has to measure and on state that changes between frames (pins
    // appear and vanish with tweaks, the menu moves while dragged), so a script asking "where is the
    // notefeed" before those ran would get last frame's answer and lag every change by a frame.
    // Running here, it reads geometry that is already final for this frame.
    //
    // The cost is one frame of latency on toasts a script raises - notefeed has already drawn by the
    // time tw.notify() reaches it - which is invisible against a 3-second toast that fades in anyway.
    tw::lua::host::draw_frame();

    // After the menu, unlike the update() above: the skybox parameter panel docks to the menu's
    // window rectangle, which is only final once the menu has drawn, and has to land above it where
    // the two overlap.
    tw::skybox::ui::draw_windows();

    tw::framework::imgui_backend::render();
}
} // namespace tw::ui
