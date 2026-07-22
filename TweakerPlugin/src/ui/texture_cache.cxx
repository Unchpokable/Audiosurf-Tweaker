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

#include "libstb/stb_image.h"

namespace
{
tw::ui::texture_cache::upload_fn g_upload = nullptr;
std::unordered_map<std::string, ImTextureID> g_cache;
} // namespace

namespace tw::ui::texture_cache
{
void set_backend(upload_fn fn) noexcept
{
    g_upload = fn;
}

ImTextureID get_or_load(std::string_view resource_key)
{
    const std::string key(resource_key);
    if(const auto it = g_cache.find(key); it != g_cache.end()) {
        return it->second;
    }

    if(g_upload == nullptr) {
        return ImTextureID_Invalid;
    }

    const auto res = tw::resource::get_resource(tw::resource::type::texture, resource_key);
    if(!res || res->bytes.empty() || res->bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        return ImTextureID_Invalid;
    }

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(res->bytes.data()),
        static_cast<int>(res->bytes.size()),
        &width,
        &height,
        &components,
        STBI_rgb_alpha);
    if(pixels == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }

    const ImTextureID tex = g_upload(pixels, width, height);
    stbi_image_free(pixels);

    g_cache.emplace(key, tex);
    return tex;
}

void clear() noexcept
{
    g_cache.clear();
}
} // namespace tw::ui::texture_cache
