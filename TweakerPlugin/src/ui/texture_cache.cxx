#include "ui/texture_cache.hxx"

// No TweakerPlugin PCH here (this TU is shared with smoke_test, which doesn't use it) - windows.h
// pulled in explicitly, same convention as smoke/main.cxx, since resource.hxx's HMODULE-typed
// declarations need it in scope.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// resource.hxx uses std::span/std::expected without including them itself (relies on the
// TweakerPlugin PCH in DLL consumers) - explicit here, same convention as smoke/main.cxx.
#include <climits>
#include <expected>
#include <span>
#include <string>
#include <unordered_map>

#include "resource/resource.hxx"

#include "ui/gpu_texture.hxx"

#include "libstb/stb_image.h"

namespace
{
// Transparent hash/equality so get_or_load() can probe the map with the std::string_view it was
// handed, instead of materializing a std::string for every lookup. That matters because this is a
// per-frame call: watermark::update() asks for its icon on every single frame, and the resource
// keys ("textures/TweakerIcon-5.png") are comfortably past MSVC's 15-char small-string buffer, so
// the old `const std::string key(resource_key)` was a heap allocation per frame per icon.
struct transparent_string_hash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept
    {
        return std::hash<std::string_view> {}(key);
    }
};

std::unordered_map<std::string, ImTextureID, transparent_string_hash, std::equal_to<>> g_cache;
} // namespace

namespace tw::ui::texture_cache
{
ImTextureID get_or_load(std::string_view resource_key)
{
    // Heterogeneous lookup: no std::string is constructed unless this is a genuine miss and we are
    // about to insert.
    if(const auto it = g_cache.find(resource_key); it != g_cache.end()) {
        return it->second;
    }

    // Not cached as a failure: no upload backend yet just means the D3D9/GL device hasn't bound,
    // which the very next frame may fix. Caching Invalid here would make the icon never appear.
    if(!gpu_texture::has_backend()) {
        return ImTextureID_Invalid;
    }

    const auto res = tw::resource::get_resource(tw::resource::type::texture, resource_key);
    if(!res || res->bytes.empty() || res->bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        // Cached as a failure, unlike the case above: a key that isn't in the packed resources, or
        // is too large to decode, will still not be there next frame. Without this the caller
        // re-runs the resource lookup every frame forever - and callers here are per-frame.
        g_cache.emplace(resource_key, ImTextureID_Invalid);
        return ImTextureID_Invalid;
    }

    const auto& bytes = res->bytes;

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &components,
        STBI_rgb_alpha);
    if(pixels == nullptr || width <= 0 || height <= 0) {
        // Same reasoning: undecodable bytes stay undecodable, so don't re-run stb_image per frame.
        g_cache.emplace(resource_key, ImTextureID_Invalid);
        return ImTextureID_Invalid;
    }

    const ImTextureID tex = gpu_texture::upload(pixels, width, height);
    stbi_image_free(pixels);

    g_cache.emplace(resource_key, tex);
    return tex;
}

void clear() noexcept
{
    g_cache.clear();
}
} // namespace tw::ui::texture_cache
