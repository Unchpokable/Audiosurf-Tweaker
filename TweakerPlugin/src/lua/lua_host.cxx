#include "pch.hxx"

#include "lua/lua_host.hxx"

#include "engine/engine_state.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_config.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"
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

    if(!tw::lua::vm::initialize()) {
        return;
    }

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
    tw::lua::vm::shutdown();
    tw::lua::registry::clear();
    tw::lua::diag::clear_all();
}

void draw_frame() noexcept
{
    if(tw::lua::vm::state() == nullptr) [[unlikely]] {
        return;
    }

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

    tw::lua::sched::draw_frame();

    io.ConfigErrorRecoveryEnableAssert = previous_assert;
    io.ConfigErrorRecoveryEnableTooltip = previous_tooltip;

    ImGui::ErrorRecoveryTryToRecoverState(&saved);
    ImGui::PopID();
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
