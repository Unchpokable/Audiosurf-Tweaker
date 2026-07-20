#include "pch.hxx"

#include "framework/channel_hook.hxx"
#include "framework/d3d9_hooks.hxx"

#include "plugin/globals.hxx"
#include "plugin/load.hxx"

#include "resource/resource.hxx"

#include "ui/ui_main.hxx"

namespace tw::plugin
{
void load_thread(void* module_handle)
{
    globals::module_handle = static_cast<HMODULE>(module_handle);

    tw::resource::initialize(globals::module_handle);
    tw::framework::install_channel_hook();
    tw::framework::d3d9::install_d3d9_hooks();
    tw::ui::initialize();
}
} // namespace tw::plugin
