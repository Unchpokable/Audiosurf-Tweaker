#pragma once

namespace tw::framework::d3d9
{
using ui_plugin_draw_fn = void (*)(IDirect3DDevice9* device);

bool install_d3d9_hooks();

void attach_ui_plugin(ui_plugin_draw_fn fn);
void detach_ui_plugin();
} // namespace tw::framework::d3d9
