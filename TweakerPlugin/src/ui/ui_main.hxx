#pragma once

namespace tw::ui
{
void initialize() noexcept;
void shutdown() noexcept;
} // namespace tw::ui

namespace tw::ui
{
void draw_frame(IDirect3DDevice9* device);
} // namespace tw::ui
