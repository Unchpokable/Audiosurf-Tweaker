#pragma once

namespace ui
{
void render_ui(IDirect3DDevice9* device);
}

namespace ui
{
void drop_imgui();
bool setup_imgui(HWND hwnd, IDirect3DDevice9* device);
bool reinit_imgui(HWND hwnd, IDirect3DDevice9* device);

void backend_invalidate_objects();
bool backend_create_objects();
}
