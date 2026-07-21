#include "pch.hxx"

#include "ui/ui_main.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/imgui_backend.hxx"
#include "framework/wndproc_hub.hxx"

#include "ipc/overlay_ipc.hxx"

#include "plugin/quest3d_state.hxx"

#include "ui/overlay_state.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"

namespace tw::ui
{
void initialize() noexcept
{
    tw::framework::d3d9::attach_ui_plugin(draw_frame);
    tw::framework::d3d9::attach_device_reset_listener(
        &tw::framework::imgui_backend::on_lost_device, &tw::framework::imgui_backend::on_reset_device);
    tw::framework::wndproc::subscribe_all(&tw::framework::imgui_backend::wndproc_bridge);

    tw::ui::plugins::statics::watermark::initialize();
    tw::ui::plugins::statics::pins::initialize();
    tw::ui::plugins::statics::notefeed::initialize();
    tw::ui::plugins::interactive::menu::initialize();

    tw::ipc::initialize();
}

void shutdown() noexcept
{
    tw::framework::d3d9::detach_ui_plugin();
    tw::framework::d3d9::detach_device_reset_listener();

    tw::ui::plugins::interactive::menu::shutdown();
    tw::ui::plugins::statics::notefeed::shutdown();
    tw::ui::plugins::statics::pins::shutdown();
    tw::ui::plugins::statics::watermark::shutdown();

    tw::framework::imgui_backend::shutdown();
    tw::ipc::shutdown();
}
} // namespace tw::ui

namespace
{
// Owned by the render thread only. Updated opportunistically once per frame - see
// tw::ui::overlay_state::refresh() for why this never blocks even under contention.
tw::ui::overlay_state::cache g_overlay_cache;

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
        tw::ui::plugins::statics::notefeed::push("Skin applied: " + g_overlay_cache.current_skin_name);
    }
    g_notified_skin = g_overlay_cache.current_skin_name;

    for(std::size_t i = 0; i < g_notified_tweaks.size(); ++i) {
        if(g_notified_tweaks[i] != g_overlay_cache.tweak_enabled[i]) {
            const auto id = tw::ui::overlay_state::all_tweak_ids()[i];
            const bool now = g_overlay_cache.tweak_enabled[i] != 0;
            std::string text(tw::ui::overlay_state::tweak_display_name(id));
            text += now ? " enabled" : " disabled";
            tw::ui::plugins::statics::notefeed::push(text);
        }
    }
    g_notified_tweaks = g_overlay_cache.tweak_enabled;
}
} // namespace

namespace tw::ui
{
void draw_frame(IDirect3DDevice9* device)
{
    if(!tw::framework::imgui_backend::is_initialized()) {
        // First frame with a bound device/window - bind_device() in d3d9_hooks.cxx has already
        // installed the wndproc hook on tw::plugin::quest3d::g_game_handle by the time EndScene
        // (and therefore this function) is ever called, so it's populated here.
        if(!tw::framework::imgui_backend::initialize(device, tw::plugin::quest3d::g_game_handle)) {
            return;
        }
    }

    // Non-blocking: on contention (the IPC thread is mid-write) this just keeps last frame's
    // snapshot.
    if(tw::ui::overlay_state::refresh(g_overlay_cache)) {
        notify_overlay_state_changes();
    }

    tw::framework::imgui_backend::new_frame();

    tw::ui::plugins::statics::watermark::update();
    tw::ui::plugins::statics::pins::update(g_overlay_cache);
    tw::ui::plugins::statics::notefeed::update();
    tw::ui::plugins::interactive::menu::update(g_overlay_cache);

    tw::framework::imgui_backend::render();
}
} // namespace tw::ui
