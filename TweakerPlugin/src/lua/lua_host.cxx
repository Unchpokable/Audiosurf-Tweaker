#include "pch.hxx"

#include "lua/lua_host.hxx"

#include "engine/engine_state.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/api/api_hud.hxx"
#include "lua/lua_config.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_log.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_remote.hxx"
#include "lua/lua_sched.hxx"
#include "lua/lua_storage.hxx"
#include "lua/lua_tabs.hxx"
#include "lua/lua_vm.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/paths.hxx"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
// engine\TweakerStuff\Scripts - see plugin/paths, which also creates it.
std::filesystem::path script_directory() noexcept
{
    const std::filesystem::path& directory = tw::plugin::paths::scripts_dir();

    std::error_code ec;
    if(directory.empty() || !std::filesystem::is_directory(directory, ec)) {
        return {};
    }

    return directory;
}
} // namespace

namespace tw::lua::host
{
void initialize() noexcept
{
    if(tw::lua::vm::state() != nullptr) {
        return;
    }

    // No graph setup here: the entry points into HighPoly.dll are resolved once, by engine_symbols,
    // when the frame spine goes in, and the channel layer is src/engine/. The VM is useful (logging,
    // HUD) whether or not any of that came up.
    const std::filesystem::path directory = script_directory();
    if(directory.empty()) {
        TW_LOG_INFO("lua_host: no TweakerStuff\\Scripts directory - scripting stays idle");
        return;
    }

    // The script log for this session, before the VM: a VM that does not come up is the first thing it should say.
    // Only when there is a Scripts directory at all - a game without scripts gets no file about them.
    tw::lua::log::open(tw::plugin::paths::logs_dir(), std::format("TweakerPlugin {}, {}", TW_PLUGIN_VERSION, LUAJIT_VERSION));

    if(!tw::lua::vm::initialize()) {
        tw::lua::log::write(-1, "fatal", {}, std::format("the scripting layer did not start: {}", tw::lua::vm::last_error()));
        tw::lua::log::close();
        return;
    }

    tw::lua::log::write(-1, "info", {}, std::format("scripting API {}", tw::lua::vm::api_version()));

    tw::lua::config::load(tw::plugin::paths::config_file(L"scripts.cfg"));

    // The one engine-layer setting that lives in the scripts file, because it is about when scripts
    // start rather than about the engine. Absent lines leave the built-in value alone, so the usual
    // case configures nothing.
    if(const int frames = tw::lua::config::settle_frames(), milliseconds = tw::lua::config::settle_ms();
        frames > 0 || milliseconds > 0) {
        tw::engine::state::configure(frames > 0 ? frames : tw::engine::state::settle_frames(),
            milliseconds > 0 ? milliseconds : tw::engine::state::settle_ms());
        TW_LOG_INFO("lua_host: settle window from scripts.cfg - {} frame(s), {} ms",
            tw::engine::state::settle_frames(),
            tw::engine::state::settle_ms());
    }

    tw::lua::sched::initialize();

    // Before any script runs: a body may read its settings.
    tw::lua::storage::initialize(tw::plugin::paths::script_data_dir());

    // The other half of the group registry's job: a subscription whose group went away is put back
    // into waiting rather than left pointing at freed memory, and attached again when it returns.
    tw::lua::api::install_engine_listeners();

    tw::lua::registry::load_all(directory);

    TW_LOG_INFO("lua_host: {} ({} of {} script(s) loaded)", LUAJIT_VERSION, tw::lua::registry::loaded_count(), tw::lua::registry::count());
}

void shutdown() noexcept
{
    if(tw::lua::vm::state() == nullptr) {
        return;
    }

    // Vtables first: the thunk they point at lives in this image, and a channel still routed
    // through it after the VM is gone would call into a dispatcher with no state behind it.
    tw::lua::api::clear_subscriptions();

    tw::lua::sched::shutdown();
    tw::lua::tabs::clear();

    // After the scripts are gone from the scheduler, before the VM: whatever they wrote last is written now.
    tw::lua::storage::shutdown();
    tw::lua::vm::shutdown();
    tw::lua::registry::clear();
    tw::lua::diag::clear_all();
    tw::lua::log::close();
}

void draw_frame() noexcept
{
    // The host's commands first, before any script draws - see lua_remote.hxx. Also where whatever
    // changed since the last frame (a click on the Scripts tab, a suspension) goes out to the host.
    tw::lua::remote::pump();

    if(tw::lua::vm::state() == nullptr) [[unlikely]] {
        return;
    }

    // What the storage writer could not write, into the scripts' messages.
    tw::lua::storage::pump();

    // The scripts' tabs as the menu should show them from the next frame on - after the host's commands, which can
    // reload a script, and before this frame's callbacks, which cannot declare anything (lua_tabs).
    tw::lua::tabs::begin_frame();

    // The backstop around the whole dispatch, not one per script - see lua_sched.hxx for why a
    // script cannot leave an ImGui stack pushed and so needs no snapshot of its own. ID scope first,
    // then the recovery snapshot: recovery restores the ID stack to whatever depth it was at when the
    // snapshot was taken, so taking it *after* the push keeps our own PopID balanced either way.
    ImGui::PushID("tw_lua");

    ImGuiErrorRecoveryState saved;
    ImGui::ErrorRecoveryStoreState(&saved);

    // A script's mistake is not a bug in the overlay, and the game is not a place to assert or to
    // pop a diagnostic tooltip over.
    ImGuiIO& io = ImGui::GetIO();
    const bool previous_assert = io.ConfigErrorRecoveryEnableAssert;
    const bool previous_tooltip = io.ConfigErrorRecoveryEnableTooltip;
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;

    // What is taken on the HUD starts over every frame, with the chrome as the overlay laid it out just now
    // (tw.hud.occupied - lua_hud_space).
    tw::lua::api::hud_begin_frame();
    tw::lua::sched::draw_frame();

    io.ConfigErrorRecoveryEnableAssert = previous_assert;
    io.ConfigErrorRecoveryEnableTooltip = previous_tooltip;

    ImGui::ErrorRecoveryTryToRecoverState(&saved);
    ImGui::PopID();

    // Last: a tab dropped this frame - a Reload from the Scripts tab, inside the menu's frame - is freed only now.
    tw::lua::tabs::end_frame();
}

bool is_running() noexcept
{
    return tw::lua::vm::state() != nullptr;
}

std::string_view last_error() noexcept
{
    return tw::lua::vm::last_error();
}
} // namespace tw::lua::host
