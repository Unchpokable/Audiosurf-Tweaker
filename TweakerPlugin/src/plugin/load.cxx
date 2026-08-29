#include "pch.hxx"

#include "framework/channel_hook.hxx"
#include "framework/d3d9_hooks.hxx"
#include "framework/dinput8_hooks.hxx"
#include "framework/texture_hook.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"
#include "plugin/load.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_ui.hxx"
#include "skybox/skybox.hxx"

#include "ui/ui_main.hxx"

namespace
{
// Overlay single-instance guard, the in-process half of the host's inject guard (see
// TweakerUI/Core/OverlayHelper.cs and Docs/Internal/overlay-protocol.md). The name is deliberately
// free of any version or path component: a newer host must be able to detect an older plugin
// sitting in the game, and the file may well have been renamed. Scoped by the host process id so
// the answer is about *this* game process specifically.
//
// The handle is never released - it dies with the game process, which is exactly the lifetime being
// guarded, and is the same self-cleaning property the host-side guards rely on.
HANDLE g_instance_mutex = nullptr;

bool claim_single_instance()
{
    std::wstring name = L"Local\\AudiosurfTweaker.Overlay.";
    name += std::to_wstring(::GetCurrentProcessId());

    g_instance_mutex = ::CreateMutexW(nullptr, FALSE, name.c_str());
    if(g_instance_mutex == nullptr) {
        // Cannot prove presence either way. Carrying on is the lesser evil: the host only ever
        // reaches an injection after its own handshake probe went unanswered, so a live first copy
        // would already have stopped it from getting here.
        TW_LOG_WARNING("load: CreateMutexW failed ({}) - proceeding without the single-instance guard", ::GetLastError());
        return true;
    }

    if(::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(g_instance_mutex);
        g_instance_mutex = nullptr;
        return false;
    }

    return true;
}
} // namespace

namespace tw::plugin
{
void load_thread(void* module_handle)
{
    globals::module_handle = static_cast<HMODULE>(module_handle);

    // First thing on the thread: everything below is worth a log line, and none of it can be
    // logged before the sinks exist.
    diagnostics::initialize();
    TW_LOG_INFO("load: bootstrap thread started, module={}", module_handle);

    // Before anything at all is wired up: a second copy in the same process would install a second
    // set of Detours over EndScene/CallChannel and a second WndProc subscription, which is a crash,
    // not a degraded overlay. Bailing out here leaves this copy mapped but completely inert - no
    // hooks, no threads, no state - rather than risking FreeLibraryAndExitThread from the very
    // thread the injector is waiting on.
    if(!claim_single_instance()) {
        TW_LOG_WARNING("load: another TweakerPlugin instance is already live in this process - this copy stays inert");
        return;
    }

    if(!tw::resource::initialize(globals::module_handle)) {
        TW_LOG_ERROR("load: resource::initialize failed - fonts and icons will be missing");
    }

    // Wire up every listener/subscriber and bring the UI + IPC subsystems online BEFORE any hook
    // goes live. install_d3d9_hooks() makes hk_end_scene/hk_create_device callable immediately on
    // the game's render thread; if that thread reached bind_device() -> on_device_bound while these
    // listeners were still null, the device would bind with no ImGui init and never retry (the bind
    // is one-shot per device/window pair). Ordering the hooks last also publishes these writes to
    // the render thread safely: DetourTransactionCommit() inside the install calls suspends and
    // resumes every other thread, which is a full barrier on the pointers written above.
    tw::ui::initialize();

    // Same "subscribe before the hooks go live" rule as tw::ui::initialize() above: this registers
    // a device bind/unbind listener, a device reset listener and the draw interceptor, all of which
    // the render thread starts calling the instant install_d3d9_hooks() commits.
    tw::skybox::initialize();

    // After tw::ui::initialize() (which builds the menu) and before the first frame (which builds
    // the tab strip) - see menu::set_extra_tab.
    tw::skybox::ui::initialize();

    if(!tw::framework::install_channel_hook()) {
        TW_LOG_WARNING("load: channel hook not installed - Quest3D engine pointer will stay null");
    }

    // Best-effort here, and retried from the skybox module's device bind listener: the game loads
    // channel DLLs on demand, so an injection early enough can beat the Texture channel into the
    // process. By the time there is a D3D9 device, it is certainly mapped.
    tw::framework::texture::install_texture_hook();

    tw::framework::d3d9::install_d3d9_hooks();
    tw::framework::dinput::install_hooks();

    TW_LOG_INFO("load: bootstrap complete");
}
} // namespace tw::plugin
