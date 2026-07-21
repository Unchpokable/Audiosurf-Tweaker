#pragma once

#include <imgui.h>

#include <string_view>

// Renderer-agnostic icon loading, shared by the DLL (D3D9) and smoke_test (OpenGL) - see
// Docs/Internal/tweaker-plugin-widgets.md. Decoding (stb_image) and the by-key cache live here;
// only the final upload step is renderer-specific, so callers plug that in once via set_backend().
namespace tw::ui::texture_cache
{
using upload_fn = ImTextureID (*)(const unsigned char* rgba, int width, int height);

// Must be called once before any get_or_load(). The DLL registers a D3D9 uploader (see
// framework/imgui_backend.cxx), smoke_test registers a GL uploader (see smoke/main.cxx).
void set_backend(upload_fn fn) noexcept;

// Decodes + uploads + caches by resource key (e.g. "textures/TweakerIcon-1.png", matching
// tw::resource's key format). Returns ImTextureID_Invalid if the resource is missing, fails to
// decode, or no backend is registered yet - callers should treat that as "skip the icon", not an
// error worth asserting on (matches list_item's existing ImTextureID_Invalid handling).
[[nodiscard]] ImTextureID get_or_load(std::string_view resource_key);

// Drops every cached ImTextureID without releasing the underlying renderer resource - callers own
// that (see framework/imgui_backend.cxx, which clears this right after a device change makes the
// cached D3D9 textures dangling, since the device itself already destroyed them). The next
// get_or_load() for a given key re-decodes and re-uploads through the current backend.
void clear() noexcept;
} // namespace tw::ui::texture_cache
