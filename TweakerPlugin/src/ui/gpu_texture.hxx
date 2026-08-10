#pragma once

#include <imgui.h>

// The one place the renderer-specific half of texture handling is plugged in. Everything above it
// (ui/texture_cache.cxx for raster assets, ui/image/svg.cxx for vector ones) produces tightly-packed
// RGBA8 and hands it here, so the DLL (D3D9) and smoke_test (OpenGL) share every line of that code
// and differ only in this pair of function pointers - see Docs/Internal/tweaker-plugin-widgets.md.
namespace tw::ui::gpu_texture
{
using upload_fn = ImTextureID (*)(const unsigned char* rgba, int width, int height);
using release_fn = void (*)(ImTextureID tex);

// Must be called once before any upload(). The DLL registers the D3D9 pair (see
// framework/imgui_backend.cxx), smoke_test the GL pair (see smoke/main.cxx).
void set_backend(upload_fn upload, release_fn release) noexcept;

// False until a renderer has bound. Callers use this to tell "no device yet, try again next frame"
// apart from "this asset is broken" - the first must not be cached as a failure.
[[nodiscard]] bool has_backend() noexcept;

// `rgba` must be tightly packed (stride == width * 4), straight (non-premultiplied) alpha.
// Returns ImTextureID_Invalid if no backend is registered or the upload failed.
[[nodiscard]] ImTextureID upload(const unsigned char* rgba, int width, int height) noexcept;

// Destroys a texture obtained from upload(). No-op on ImTextureID_Invalid or with no backend.
// Only valid while the renderer that produced `tex` is still alive: once a device has been
// replaced, its textures are already gone and the handles must simply be dropped, not released
// (see texture_cache::clear() and image::svg::invalidate()).
void release(ImTextureID tex) noexcept;
} // namespace tw::ui::gpu_texture
