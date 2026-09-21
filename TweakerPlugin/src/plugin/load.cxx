#include "pch.hxx"

#include "plugin/load.hxx"

#include "framework/channel_hook.hxx"
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

void install_late_hooks()
{
    const bool channel_ok = tw::framework::install_channel_hook();
    TW_BOOT_LOG("late: CallChannel {}", channel_ok ? "hooked" : "hook FAILED - scripts will not see the engine");
    if(!channel_ok) {
        TW_LOG_WARNING("load: channel hook not installed - Quest3D engine pointer will stay null");
    }

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

    // lua_channels resolves HighPoly.dll entry points here, and the VM has to exist before the first frame
    // calls into it. Script handles resolve lazily anyway, because the EngineInterface pointer the channel
    // hook captures arrives late (see lua-scripting.md §7).
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
            TW_BOOT_LOG("stage 2: still no device bound after {:.0f} s (d3d9.dll {}, Direct3DCreate9 hook {})",
                elapsed_ms() / 1000.0,
                ::GetModuleHandleW(k_d3d9_module.data()) != nullptr ? "mapped" : "not mapped",
                g_startup == startup::early ? (g_d3d9_hooked.load() ? "installed" : "not installed") : "not used (late)");
        }
    }

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

        // Nobody runs a channel while the engine is still scanning for them, and this DllMain is part of
        // that scan - so the patch goes in without suspending anyone.
        const bool channel_ok = framework::install_channel_hook(framework::detour::suspend::none);
        TW_BOOT_LOG("stage 0: CallChannel {}", channel_ok ? "hooked" : "hook FAILED - scripts will not see the engine");

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
