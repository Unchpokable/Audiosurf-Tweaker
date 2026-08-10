#pragma once

#include <imgui.h>

#include <array>
#include <cstdint>
#include <string_view>

// Eagerly-baked SVG icon repository. Unlike ui/texture_cache (raster, lazy, one texture per asset),
// this module rasterizes every packed SVG (resource::type::vector, see resource/CMakeLists.txt) at a
// fixed set of sizes in one go, so a draw call is a hash lookup and never a rasterization.
//
// Assets are authored monochrome (white + alpha) - colour comes from the theme at draw time, via
// the ImU32 tint ImGui multiplies into the image (see widgets/detail/draw.hxx's
// add_image_keep_aspect). See Docs/Internal/tweaker-plugin-widgets.md.
//
// Renderer-agnostic: uploads go through ui/gpu_texture, so this works identically in the DLL (D3D9)
// and in smoke_test (OpenGL).
namespace tw::ui::image::svg
{
// Sizes baked up front, in pixels. Single source of truth - image::sz16/24/32 below mirror these,
// and anything else goes through at().
inline constexpr std::array<int, 3> k_baked_sizes { 16, 24, 32 };

inline constexpr std::uint32_t k_invalid_slot = 0xFFFFFFFFu;

// A handle to one packed SVG plus its already-uploaded textures.
//
// The ImTextureID fields are a SNAPSHOT: they go stale the moment the render device is replaced
// (invalidate() -> re-bake). Call get_resource() where you draw - it is a heterogeneous hash lookup
// with no allocation, cheap enough for a per-frame call - rather than caching an image across
// frames. Same rule as theme colours: read it every frame, don't hold it.
struct image final {
    ImTextureID sz16 = ImTextureID_Invalid;
    ImTextureID sz24 = ImTextureID_Invalid;
    ImTextureID sz32 = ImTextureID_Invalid;

    // Any pixel size. A size outside k_baked_sizes is rasterized and uploaded on first ask and kept
    // from then on - one frame pays for it, the rest read the cache. Returns ImTextureID_Invalid for
    // an empty handle, a non-positive size, or a failed bake.
    [[nodiscard]] ImTextureID at(int px) const noexcept;

    [[nodiscard]] bool valid() const noexcept
    {
        return m_slot != k_invalid_slot;
    }

private:
    friend image get_resource(std::string_view resource_key) noexcept;

    std::uint32_t m_slot = k_invalid_slot; // index into the repository, so at() can bake on demand
};

// Indexes every packed SVG by resource key. No GPU work - safe to call from the load thread, before
// any device exists.
void initialize() noexcept;

// Releases every uploaded texture. Must run while the renderer that produced them is still alive
// (in tw::ui::shutdown(), which happens before imgui_backend::shutdown()).
void shutdown() noexcept;

// The render device changed: every ImTextureID handed out so far is dangling. Drops them without
// releasing (the departed device already destroyed them) and marks the repository for a re-bake on
// the next update().
void invalidate() noexcept;

// Call once per frame, at the top of the frame, with a renderer bound. A no-op (one bool test)
// unless a bake is pending.
void update() noexcept;

// Key is the full resource path, e.g. "icons/skin.svg". A miss returns an empty image whose every
// texture is ImTextureID_Invalid, which callers can pass straight to add_image_keep_aspect - it
// draws nothing rather than failing.
[[nodiscard]] image get_resource(std::string_view resource_key) noexcept;
} // namespace tw::ui::image::svg
