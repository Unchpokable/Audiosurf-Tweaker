#include "pch.hxx"

#include "plugin/load.hxx"

#include "engine/engine_control.hxx"
#include "engine/engine_frame.hxx"
#include "engine/engine_groups.hxx"
#include "engine/engine_state.hxx"
#include "engine/engine_symbols.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/dinput8_hooks.hxx"
#include "framework/loader_watch.hxx"
#include "framework/ready.hxx"
#include "framework/texture_hook.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"
#include "plugin/paths.hxx"
#include "plugin/presence.hxx"

#include "ipc/overlay_ipc.hxx"

#include "resource/resource.hxx"

#include "lua/lua_host.hxx"
#include "lua/lua_ui.hxx"

#include "skybox/sky_ui.hxx"
#include "skybox/skybox.hxx"

#include "ui/ui_main.hxx"

namespace
{
enum class startup : std::uint8_t {
    early,
    late,
};

constexpr std::wstring_view k_game_executable = L"QuestViewer.exe";
constexpr std::wstring_view k_d3d9_module = L"d3d9.dll";

// Quest3D's Texture channel, Aco_DX8_Texture - see framework/texture_hook.cxx.
constexpr std::wstring_view k_texture_channel_module = L"BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll";

// Past this, stage 2 writes down what it is still waiting for. Nothing times out: an offline plugin has
// nowhere to be, and a game sitting in a long load is not a failure.
constexpr auto k_slow_stage = std::chrono::seconds { 30 };

startup g_startup = startup::late;

std::atomic<bool> g_d3d9_hooked { false };

// Up from the first device bind onwards - stage 2.
std::atomic<bool> g_device_bound { false };

bool equals_ignore_case(std::wstring_view a, std::wstring_view b) noexcept
{
    return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

std::string module_path(HMODULE module) noexcept
{
    std::array<wchar_t, MAX_PATH> buffer {};
    const DWORD length = ::GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    return tw::plugin::paths::to_utf8(std::filesystem::path { std::wstring_view { buffer.data(), length } });
}

// --- loader notifications (early load) -------------------------------------------------------------------
//
// Under the loader lock, on whichever thread loads the module. Only what §4.2 allows: pin, one detour
// transaction without suspending anyone, the lifecycle log.

void on_d3d9_loaded(HMODULE module)
{
    // HighPoly maps d3d9.dll as a dependency of a channel it is only probing, frees it, and maps it again.
    // The pin is what keeps that from unmapping the image the hook is about to go into (Р-25).
    if(!tw::framework::loader_watch::pin(module)) {
        TW_BOOT_LOG("module: d3d9.dll loaded at {} but cannot be pinned (error {}) - not hooked, waiting for the next load",
            static_cast<void*>(module),
            ::GetLastError());
        return;
    }

    if(g_d3d9_hooked.exchange(true)) {
        TW_BOOT_LOG("module: d3d9.dll loaded again at {} although pinned - anomaly, hooks left alone", static_cast<void*>(module));
        return;
    }

    const bool ok = tw::framework::d3d9::hook_direct3d_create9_from_loader(module);
    TW_BOOT_LOG("module: d3d9.dll loaded at {}, pinned, Direct3DCreate9 {}",
        static_cast<void*>(module),
        ok ? "hooked" : "hook FAILED - no overlay and no sky this session");
}

void on_d3d9_unloaded(HMODULE module)
{
    TW_BOOT_LOG("module: d3d9.dll UNLOADED at {} despite the pin - anomaly", static_cast<void*>(module));
}

void on_texture_channel_loaded(HMODULE module)
{
    if(!tw::framework::loader_watch::pin(module)) {
        TW_BOOT_LOG("module: Texture channel loaded at {} but cannot be pinned (error {}) - not hooked, waiting for the next load",
            static_cast<void*>(module),
            ::GetLastError());
        return;
    }

    TW_BOOT_LOG("module: Texture channel loaded at {}, pinned", static_cast<void*>(module));
    tw::framework::texture::install_texture_hook_from_loader(module);
}

void on_texture_channel_unloaded(HMODULE module)
{
    TW_BOOT_LOG("module: Texture channel UNLOADED at {} despite the pin - anomaly", static_cast<void*>(module));
}

constexpr std::array<tw::framework::loader_watch::watch, 2> k_watches { {
    { k_d3d9_module, &on_d3d9_loaded, &on_d3d9_unloaded },
    { k_texture_channel_module, &on_texture_channel_loaded, &on_texture_channel_unloaded },
} };

// --- stage 2 ----------------------------------------------------------------------------------------------

// Registered last, so it runs after every other bind listener - the ImGui backend included. On the render
// thread, inside bind_device: initialisation, where SetEvent and the lifecycle log are allowed (§4.5).
void on_device_bound(IDirect3DDevice9* device, HWND hwnd)
{
    if(g_device_bound.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    // bind_device has hooked the game window's WndProc just before calling its listeners, so TW_OVL can be
    // received from here on.
    tw::plugin::presence::signal_ready();
    TW_BOOT_LOG("stage 2: first device bound (device {}, hwnd {}) - Ready signalled", static_cast<void*>(device), static_cast<void*>(hwnd));
}

// --- stage 1 and 3 ----------------------------------------------------------------------------------------

// The frame spine: `EngineControl::EngineLoop`, where the engine pointer and the per-frame tick both
// come from (Docs/Internal/reversing-journal-boot.md §1.2).
//
// There is no fallback. The detour on the empty `A3d_Channel::CallChannel` that used to capture the
// engine pointer is gone, and with it a trampoline on every call of 22 655 channels for the whole
// session. It was kept behind this call through Ф1 in case the class had been identified wrongly;
// both load modes are now confirmed on the real game (see the plan's Ф1 section), and the one failure
// it could have covered - the symbol missing from a shipped HighPoly.dll - cannot happen.
//
// The failure it could NOT cover, and still cannot: "installed, but the object is not an
// EngineControl". That is checked on the first frame, on the engine thread, and reported on the
// `engine: first EngineLoop` line below. Recovering from it automatically was deliberately not
// built - reading the log once on a real game is the mitigation.
//
// `stage` and `threads` travel together because they are one decision seen twice: stage 0 means
// DllMain, which means suspend::none, and "late" means the startup thread, which means
// suspend::others. The reasoning is in engine_control::install().
void install_frame_spine(const char* stage, tw::framework::detour::suspend threads)
{
    if(tw::engine::control::install(threads)) {
        TW_BOOT_LOG("{}: EngineLoop hooked - engine pointer and frame tick come from the spine", stage);
    }
    else {
        const std::string_view reason = tw::engine::symbols::missing();
        TW_BOOT_LOG("{}: EngineLoop hook FAILED ({}) - no engine pointer, no frame tick, scripts stay inert",
            stage,
            reason.empty() ? std::string_view { "detour refused" } : reason);
        return;
    }

    // The group registry's two detours go in with the spine and under the same rule, because the
    // argument for `suspend::none` is the same one: at stage 0 the game is still enumerating
    // engine\channels\ - which is what loaded us - and has not loaded a single channel group yet, so
    // nobody can be inside A3d_ChannelGroup::Release. Late, the game is running and the other threads
    // are held, exactly as for EngineLoop (plugin-offline-mode.md §4.2).
    //
    // Failing here is survivable in a way failing above is not: without it the plugin still sees
    // frames and still reads the graph, it just cannot tell when a group goes away - so scripts
    // hooking a channel in a pool that is rebuilt every run are the part that breaks.
    if(tw::engine::groups::install_hooks(threads)) {
        TW_BOOT_LOG("{}: group registry hooked - handles and channel hooks survive a group unloading", stage);
        return;
    }

    const std::string_view reason = tw::engine::symbols::missing();
    TW_BOOT_LOG("{}: group registry NOT hooked ({}) - a script hooking a channel in a pool the game "
                "rebuilds each run is unsafe this session",
        stage,
        reason.empty() ? std::string_view { "detour refused" } : reason);
}

// What the spine is doing, in the words the log reader needs.
//
// **In the early load this is always called before the first engine frame, and that is normal.**
// Stage 3 finishes when the device binds, and the game creates its device while it is still
// initialising - the message pump that calls EngineLoop starts afterwards. Measured on the game:
// 140 ms between the two on one run, 800 ms on another. An earlier version of this called that state
// "installed, silent", which reads like a fault and is the healthy case; hence the wording below and
// the pointer to the line that actually carries the verdict.
const char* describe_spine()
{
    if(!tw::engine::control::installed()) {
        return "NOT INSTALLED - scripts will not see the engine";
    }

    if(!tw::engine::control::ticked()) {
        return "installed, no frame yet - look for 'engine: first EngineLoop' below";
    }

    return tw::engine::control::healthy() ? "live" : "installed, WRONG OBJECT - the detour fired on something that is not an EngineControl";
}

void install_late_hooks()
{
    // The game is already running its message pump here, so the spine goes in the way any hook into
    // live code does - with the other threads held.
    install_frame_spine("late", tw::framework::detour::suspend::others);

    // Best-effort here, and retried from the skybox module's device bind listener: the game loads
    // channel DLLs on demand, so an injection early enough can beat the Texture channel into the
    // process. By the time there is a D3D9 device, it is certainly mapped.
    tw::framework::texture::install_texture_hook();

    tw::framework::d3d9::install_d3d9_hooks();
    tw::framework::dinput::install_hooks();
}

void run_startup(HMODULE module)
{
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed_ms = [&started] {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    };

    tw::plugin::globals::module_handle = module;

    tw::plugin::diagnostics::initialize();
    TW_LOG_INFO("load: startup thread running, module={}", static_cast<void*>(module));
    TW_BOOT_LOG("stage 1: startup thread running");

    tw::plugin::paths::create_directories();

    if(!tw::resource::initialize(module)) {
        TW_LOG_ERROR("load: resource::initialize failed - fonts and icons will be missing");
        TW_BOOT_LOG("stage 1: embedded resources unavailable - fonts and icons will be missing");
    }

    // Every registration below lands in a vector or a pointer the game's threads will read. None of it is
    // visible to them before framework::ready is published at the end of this stage - until then every
    // hook only forwards - which is what makes the order here free of races in both modes.
    tw::framework::d3d9::initialize();
    tw::ui::initialize();
    tw::skybox::initialize();

    // After tw::ui::initialize() (which builds the menu) and before the first frame (which builds
    // the tab strip) - see menu::add_extra_tab.
    tw::skybox::ui::initialize();
    tw::lua::ui::initialize();

    // Before the scripting layer, and that order is load-bearing rather than tidy. The spine calls its
    // pre-frame subscribers in registration order, and a script's tick has to see this frame's state:
    // the group registry has to have rebuilt its roster and the state machine has to have decided
    // where it is before anything asks either of them.
    tw::engine::state::initialize();

    // The VM has to exist before the first frame calls into it. This is also where the scripting layer subscribes to the frame spine - which in the
    // early load is already installed and already counting frames, and is holding its subscribers back
    // until framework::ready goes up a few lines below.
    tw::lua::host::initialize();

    tw::framework::d3d9::attach_device_bind_listener(&on_device_bound, nullptr);

    tw::framework::ready::publish();
    TW_BOOT_LOG("stage 1: ready after {:.1f} ms", elapsed_ms());

    if(g_startup == startup::late) {
        install_late_hooks();
    }

    auto next_report = std::chrono::steady_clock::now() + k_slow_stage;
    while(!g_device_bound.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds { 50 });

        if(std::chrono::steady_clock::now() >= next_report) {
            next_report += k_slow_stage;
            TW_BOOT_LOG("stage 2: still no device bound after {:.0f} s (d3d9.dll {}, Direct3DCreate9 hook {}, spine {})",
                elapsed_ms() / 1000.0,
                ::GetModuleHandleW(k_d3d9_module.data()) != nullptr ? "mapped" : "not mapped",
                g_startup == startup::early ? (g_d3d9_hooked.load() ? "installed" : "not installed") : "not used (late)",
                describe_spine());
        }
    }

    // The one line worth reading when scripts do not work. "installed, silent" means the detour went into
    // something the game never calls, and "installed, wrong object" means EngineControl is not the class
    // the main loop drives - both of which are the risk Ф1 was told to check on a real game.
    TW_BOOT_LOG("stage 3: frame spine {} ({} engine frame(s))", describe_spine(), tw::engine::frame::count());

    tw::ipc::start_host_watchdog();

    TW_LOG_INFO("load: startup complete");
    TW_BOOT_LOG("stage 3: startup complete after {:.1f} ms", elapsed_ms());
}

unsigned __stdcall startup_thread_entry(void* parameter)
{
    run_startup(static_cast<HMODULE>(parameter));
    return 0;
}

void write_session_header(HMODULE module)
{
    SYSTEMTIME local {};
    ::GetLocalTime(&local);

    TW_BOOT_LOG("attach: ===== {:04}-{:02}-{:02} {:02}:{:02}:{:02} TweakerPlugin {} in pid {} =====",
        local.wYear,
        local.wMonth,
        local.wDay,
        local.wHour,
        local.wMinute,
        local.wSecond,
        TW_PLUGIN_VERSION,
        ::GetCurrentProcessId());
    TW_BOOT_LOG("attach: module '{}'", module_path(module));
}
} // namespace

namespace tw::plugin
{
void on_process_attach(HMODULE module) noexcept
{
    // A DLL in engine\channels\ is loaded by anything built on the engine, not only by the game - and before
    // the DISABLE check nothing may be touched on disk.
    if(!paths::resolve()) {
        return;
    }

    const std::filesystem::path executable_name = paths::executable_file().filename();
    if(!equals_ignore_case(executable_name.native(), k_game_executable)) {
        return;
    }

    if(paths::is_disabled()) {
        return;
    }

    const presence::claim_result claim = presence::claim();
    if(claim == presence::claim_result::already_loaded) {
        // "Load now" into a game that already has the channels\ copy, or the same DLL under two names.
        boot_log::open(paths::logs_dir(), boot_log::open_mode::guest);
        TW_BOOT_LOG("attach: another TweakerPlugin is already loaded in this process - '{}' stays inert", module_path(module));
        return;
    }

    boot_log::open(paths::logs_dir(), boot_log::open_mode::owner);
    write_session_header(module);

    if(claim == presence::claim_result::unavailable) {
        TW_BOOT_LOG("attach: presence mutex not created (error {}) - the host will not see this plugin", ::GetLastError());
    }

    // Pinned before anything that outlives DllMain goes in: the engine's channel scan frees every DLL it
    // probes right after loading it, and hooks, a loader callback or a thread left in an unmapped image
    // are a crash on the next call (§2.1, Р-10).
    if(!framework::loader_watch::pin(module)) {
        TW_BOOT_LOG("attach: cannot pin the plugin (error {}) - staying inert", ::GetLastError());
        return;
    }

    g_startup = ::GetModuleHandleW(k_d3d9_module.data()) == nullptr ? startup::early : startup::late;
    TW_BOOT_LOG("stage 0: {} load (d3d9.dll {})",
        g_startup == startup::early ? "early" : "late",
        g_startup == startup::early ? "not mapped yet" : "already mapped");

    if(g_startup == startup::early) {
        // Before returning: d3d9.dll maps ~35 ms from now, and a notification registered later would miss it.
        const bool watching = framework::loader_watch::start(k_watches);
        TW_BOOT_LOG("stage 0: loader notifications {}", watching ? "registered" : "FAILED - no overlay and no sky this session");

        // The frame spine replaces the CallChannel hook that used to go in here. Same argument for
        // suspend::none as that one had, and a stronger one: nobody can be inside EngineLoop yet either,
        // because the engine is still enumerating engine\channels\ - which is what is running this
        // DllMain - and its message pump has not started.
        install_frame_spine("stage 0", framework::detour::suspend::none);

        const bool dinput_ok = framework::dinput::install_create_hook_from_loader();
        TW_BOOT_LOG("stage 0: DirectInput8Create {}", dinput_ok ? "hooked" : "hook FAILED - the game will see input the menu eats");

        // Not expected: the channel is loaded after the scan. Its notification will never come, so the hook
        // is left to the skybox's retry on the first device bind.
        if(::GetModuleHandleW(k_texture_channel_module.data()) != nullptr) {
            TW_BOOT_LOG("stage 0: the Texture channel is already mapped - its hook waits for the first device bind");
        }
    }

    const std::uintptr_t thread = ::_beginthreadex(nullptr, 0, &startup_thread_entry, module, 0, nullptr);
    if(thread == 0) {
        // The early hooks stay in and stay pass-through: framework::ready is never published.
        TW_BOOT_LOG("attach: cannot start the startup thread (errno {}) - the plugin stays pass-through", errno);
        return;
    }

    ::CloseHandle(reinterpret_cast<HANDLE>(thread));
}
} // namespace tw::plugin
