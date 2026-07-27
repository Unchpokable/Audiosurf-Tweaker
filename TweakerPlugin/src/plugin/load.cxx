#include "pch.hxx"

#include "framework/channel_hook.hxx"
#include "framework/d3d9_hooks.hxx"
#include "framework/dinput8_hooks.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"
#include "plugin/load.hxx"

#include "resource/resource.hxx"

#include "ui/ui_main.hxx"

namespace tw::plugin
{
void load_thread(void* module_handle)
{
    globals::module_handle = static_cast<HMODULE>(module_handle);

    // First thing on the thread: everything below is worth a log line, and none of it can be
    // logged before the sinks exist.
    diagnostics::initialize();
    TW_LOG_INFO("load: bootstrap thread started, module={}", module_handle);

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

    if(!tw::framework::install_channel_hook()) {
        TW_LOG_WARNING("load: channel hook not installed - Quest3D engine pointer will stay null");
    }

    tw::framework::d3d9::install_d3d9_hooks();
    tw::framework::dinput::install_hooks();

    TW_LOG_INFO("load: bootstrap complete");
}
} // namespace tw::plugin
