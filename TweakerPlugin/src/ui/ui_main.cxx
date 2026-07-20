#include "pch.hxx"

#include "ui/ui_main.hxx"

#include "framework/d3d9_hooks.hxx"

#include "ipc/overlay_ipc.hxx"

#include "ui/overlay_state.hxx"

namespace tw::ui
{
void initialize() noexcept 
{
    tw::framework::d3d9::attach_ui_plugin(draw_frame);
    tw::ipc::initialize();
}

void shutdown() noexcept 
{
    tw::framework::d3d9::detach_ui_plugin();
    tw::ipc::shutdown();
}
} // namespace tw::ui

namespace
{
// Owned by the render thread only. Updated opportunistically once per frame - see
// tw::ui::overlay_state::refresh() for why this never blocks even under contention.
tw::ui::overlay_state::cache g_overlay_cache;
} // namespace

namespace tw::ui
{
void draw_frame(IDirect3DDevice9* /*device*/)
{
    // Non-blocking: on contention (the IPC thread is mid-write) this just keeps last frame's
    // snapshot. Once ImGui panels exist (plugin-ui-bind), they render from g_overlay_cache, never
    // from tw::ui::overlay_state directly.
    tw::ui::overlay_state::refresh(g_overlay_cache);
}
} // namespace tw::ui
