#include "ui/image/svg.hxx"

// No TweakerPlugin PCH here (this TU is shared with smoke_test, which doesn't use it) - windows.h
// pulled in explicitly, same convention as ui/texture_cache.cxx, since resource.hxx's HMODULE-typed
// declarations need it in scope.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// resource.hxx uses std::span/std::expected without including them itself (relies on the
// TweakerPlugin PCH in DLL consumers) - explicit here, same convention as ui/texture_cache.cxx.
#include <cstddef>
#include <cstring>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "resource/resource.hxx"

#include "ui/gpu_texture.hxx"

#include "libstb/stb_image_resize2.h"

#include <lunasvg.h>

namespace
{
// 1 = let lunasvg antialias straight into the target size; >1 = rasterize that many times larger
// and box it back down with stb_image_resize2. lunasvg's rasterizer is a proper scanline AA one, so
// for the simple path-based icons in assets/icons/ the direct route is normally the sharper of the
// two - supersampling mostly buys robustness for busier artwork, at the cost of a softer result.
// The smoke_test "Icons" section exists to compare the two side by side before changing this.
constexpr int k_supersample_factor = 1;

// image::sz16/sz24/sz32 are positional aliases into slot::baked. If the size list changes, those
// three fields have to change with it - fail the build rather than silently renaming what sz16 is.
static_assert(tw::ui::image::svg::k_baked_sizes.size() == 3, "image's sz16/sz24/sz32 fields mirror k_baked_sizes");
static_assert(tw::ui::image::svg::k_baked_sizes[0] == 16 && tw::ui::image::svg::k_baked_sizes[1] == 24
        && tw::ui::image::svg::k_baked_sizes[2] == 32,
    "image's sz16/sz24/sz32 fields mirror k_baked_sizes");

struct extra_size {
    int px = 0;
    ImTextureID tex = ImTextureID_Invalid;
};

struct slot {
    std::string key;
    std::span<const std::byte> bytes; // PE LockResource memory, owned by the module - never freed
    std::array<ImTextureID, tw::ui::image::svg::k_baked_sizes.size()> baked {};
    std::vector<extra_size> extra; // sizes asked for outside k_baked_sizes, incl. failed bakes
};

// Transparent hash/equality so get_resource() can probe with the std::string_view it was handed
// instead of materializing a std::string - it is called per frame per icon. Same pattern as
// resource.cxx and texture_cache.cxx.
struct transparent_string_hash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept
    {
        return std::hash<std::string_view> {}(key);
    }
};

std::vector<slot> g_slots;
std::unordered_map<std::string, std::uint32_t, transparent_string_hash, std::equal_to<>> g_index;
bool g_baked = false;

// Rasterizes `doc` into a `px` by `px` texture. Cold path (bootstrap, or the first frame that asks
// for an unbaked size), so readable beats clever here.
ImTextureID rasterize(const lunasvg::Document& doc, int px)
{
    if(px <= 0) {
        return ImTextureID_Invalid;
    }

    const int render_px = px * k_supersample_factor;
    lunasvg::Bitmap bitmap = doc.renderToBitmap(render_px, render_px);
    if(bitmap.data() == nullptr || bitmap.width() <= 0 || bitmap.height() <= 0) {
        return ImTextureID_Invalid;
    }

    // lunasvg renders premultiplied ARGB32; this turns it into the straight RGBA8 both
    // gpu_texture::upload() and stb_image_resize2's STBIR_RGBA layout expect.
    bitmap.convertToRGBA();

    const int width = bitmap.width();
    const int height = bitmap.height();
    const int stride = bitmap.stride();
    const auto row_bytes = static_cast<std::size_t>(width) * 4u;

    // upload() wants a tightly-packed buffer. renderToBitmap() normally hands back exactly that,
    // so only compact when it doesn't.
    std::vector<unsigned char> packed;
    const unsigned char* pixels = bitmap.data();
    if(stride != static_cast<int>(row_bytes)) {
        packed.resize(row_bytes * static_cast<std::size_t>(height));
        for(int y = 0; y < height; ++y) {
            std::memcpy(packed.data() + static_cast<std::size_t>(y) * row_bytes,
                bitmap.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride),
                row_bytes);
        }
        pixels = packed.data();
    }

    if constexpr(k_supersample_factor <= 1) {
        return tw::ui::gpu_texture::upload(pixels, width, height);
    }
    else {
        // Gamma-correct, alpha-weighted downscale. STBIR_RGBA (not _PM) is the right layout: the
        // alpha was already un-premultiplied by convertToRGBA() above.
        std::vector<unsigned char> scaled(static_cast<std::size_t>(px) * static_cast<std::size_t>(px) * 4u);
        if(stbir_resize_uint8_srgb(pixels, width, height, 0, scaled.data(), px, px, 0, STBIR_RGBA) == nullptr) {
            return ImTextureID_Invalid;
        }
        return tw::ui::gpu_texture::upload(scaled.data(), px, px);
    }
}

std::unique_ptr<lunasvg::Document> load_document(const slot& s)
{
    if(s.bytes.empty()) {
        return nullptr;
    }
    return lunasvg::Document::loadFromData(reinterpret_cast<const char*>(s.bytes.data()), s.bytes.size());
}

// One parse per icon, all baked sizes rendered from it.
void bake_slot(slot& s)
{
    const auto doc = load_document(s);
    if(doc == nullptr) {
        // Leaves every size at ImTextureID_Invalid - callers draw nothing rather than fail, and
        // g_baked still flips, so this isn't retried every frame.
        return;
    }

    for(std::size_t i = 0; i < tw::ui::image::svg::k_baked_sizes.size(); ++i) {
        s.baked[i] = rasterize(*doc, tw::ui::image::svg::k_baked_sizes[i]);
    }
}

void drop_textures(bool release)
{
    for(slot& s : g_slots) {
        for(ImTextureID& tex : s.baked) {
            if(release) {
                tw::ui::gpu_texture::release(tex);
            }
            tex = ImTextureID_Invalid;
        }
        if(release) {
            for(const extra_size& e : s.extra) {
                tw::ui::gpu_texture::release(e.tex);
            }
        }
        s.extra.clear();
    }
    g_baked = false;
}
} // namespace

namespace tw::ui::image::svg
{
ImTextureID image::at(int px) const noexcept
{
    if(m_slot >= g_slots.size() || px <= 0) {
        return ImTextureID_Invalid;
    }

    slot& s = g_slots[m_slot];
    for(std::size_t i = 0; i < k_baked_sizes.size(); ++i) {
        if(k_baked_sizes[i] == px) {
            return s.baked[i];
        }
    }
    for(const extra_size& e : s.extra) {
        if(e.px == px) {
            return e.tex;
        }
    }

    // Nothing is on the GPU yet (no renderer bound, or update() hasn't run this bind). Don't bake -
    // and don't record a failure either, or the size would stay broken once the device shows up.
    if(!g_baked) {
        return ImTextureID_Invalid;
    }

    const auto doc = load_document(s);
    const ImTextureID tex = doc != nullptr ? rasterize(*doc, px) : ImTextureID_Invalid;
    // Recorded even when the bake failed: a size that can't be produced now can't be produced next
    // frame either, and this is reached from draw code that would otherwise retry forever.
    s.extra.push_back({ px, tex });
    return tex;
}

void initialize() noexcept
{
    shutdown();

    const auto keys = tw::resource::list_keys(tw::resource::type::vector);
    g_slots.reserve(keys.size());
    g_index.reserve(keys.size());

    for(const std::string_view key : keys) {
        const auto res = tw::resource::get_resource(tw::resource::type::vector, key);
        if(!res || res->bytes.empty()) {
            continue;
        }

        slot s;
        s.key.assign(key);
        s.bytes = res->bytes;
        s.baked.fill(ImTextureID_Invalid);

        g_index.emplace(s.key, static_cast<std::uint32_t>(g_slots.size()));
        g_slots.push_back(std::move(s));
    }
}

void shutdown() noexcept
{
    drop_textures(true);
    g_slots.clear();
    g_index.clear();
}

void invalidate() noexcept
{
    // No release(): these textures belonged to a device that is already gone, and it took them with
    // it. Same reasoning as texture_cache::clear() - see framework/imgui_backend.cxx.
    drop_textures(false);
}

void update() noexcept
{
    if(g_baked || !gpu_texture::has_backend()) {
        return;
    }

    for(slot& s : g_slots) {
        bake_slot(s);
    }
    g_baked = true;
}

image get_resource(std::string_view resource_key) noexcept
{
    const auto it = g_index.find(resource_key);
    if(it == g_index.end()) {
        return {};
    }

    const slot& s = g_slots[it->second];
    image img;
    img.m_slot = it->second;
    img.sz16 = s.baked[0];
    img.sz24 = s.baked[1];
    img.sz32 = s.baked[2];
    return img;
}
} // namespace tw::ui::image::svg
