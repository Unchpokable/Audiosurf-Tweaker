#include "pch.hxx"

#include "skybox/sky_sprite_atlas.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
namespace atlas = tw::skybox::sprite_atlas;

IDirect3DDevice9* g_device = nullptr;
IDirect3DTexture9* g_texture = nullptr;

// The grid the atlas is currently packed in. Runtime because a package declares how many tiles it
// wants and how big they are; these are what the built-in bake asks for when nothing else has.
int g_tiles_x = 2;
int g_tiles_y = 2;
int g_tile_size = atlas::k_builtin_tile_size;
int g_tile_count = atlas::k_builtin_tiles;

// Where the atlas comes from. Three states rather than "supplied or not", because a `shader` fill
// asks for no atlas at all and that is a different answer from an empty one - see set_none().
enum class source {
    builtin,  // the bake below
    supplied, // tiles a package produced, held in g_supplied
    none,     // there is no texture; the layer's own pixel shader paints the quad
};

source g_source = source::builtin;

// Tiles a package supplied, already quantised.
//
// Kept rather than uploaded immediately because set_tiles() is called from the layer rebuild, which
// may happen with no device bound - the texture is created on the next ensure(), where there is one.
std::vector<std::uint32_t> g_supplied;

int atlas_width() noexcept
{
    return g_tiles_x * g_tile_size;
}

int atlas_height() noexcept
{
    return g_tiles_y * g_tile_size;
}

// Adopts the grid `count` tiles are laid out in. The rule itself is public - see grid_for().
void choose_grid(int count) noexcept
{
    const atlas::grid_shape shape = atlas::grid_for(count);

    g_tiles_x = shape.columns;
    g_tiles_y = shape.rows;
    g_tile_count = count;
}

// One failure is enough: a device that will not give us a 512x512 managed texture is not going to
// start, and the layer has a perfectly good "draw nothing" answer.
bool g_failed = false;

std::uint32_t hash_u32(std::uint32_t x) noexcept
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;

    return x;
}

float hash01(int x, int y, std::uint32_t seed) noexcept
{
    const std::uint32_t h = hash_u32(static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(y) * 668265263U + seed);
    return static_cast<float>(h & 0xffffffU) / static_cast<float>(0x1000000U);
}

float smoothstep01(float t) noexcept
{
    return t * t * (3.f - 2.f * t);
}

// Plain value noise. Gradient noise would be smoother, but every use of it here goes through fbm
// and then through a threshold, and at that point the difference does not survive to the screen.
float value_noise(float x, float y, std::uint32_t seed) noexcept
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);

    const auto ix = static_cast<int>(fx);
    const auto iy = static_cast<int>(fy);

    const float tx = smoothstep01(x - fx);
    const float ty = smoothstep01(y - fy);

    const float a = hash01(ix, iy, seed);
    const float b = hash01(ix + 1, iy, seed);
    const float c = hash01(ix, iy + 1, seed);
    const float d = hash01(ix + 1, iy + 1, seed);

    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// Plain fbm, with each octave's domain rotated. Both halves of that are corrections to a first
// attempt that produced faceted blobs with straight diagonal streaks instead of clouds.
//
// **Not billow.** Folding the noise about its midpoint (`abs(2n-1)`) is the usual way to get
// rounded lobes, and it is what the sky's own cloud layer uses. On a *lattice* noise it backfires:
// the fold creates a hard crease exactly along the field's zero contours, and those contours follow
// the lattice - so the creases come out as straight lines. The threshold below already produces
// lobes; taking them from a smooth field makes them round instead of creased.
//
// **Rotated per octave**, because value noise interpolated on a square grid carries that grid's
// directions in its derivative, and stacking octaves that all share the grid accumulates the
// alignment into visible axis- and diagonal-aligned streaks. Turning each octave by an angle that
// is not a fraction of a right angle leaves nothing for them to line up on.
//
// Worth being explicit that this is the opposite of the lesson from the sky's aurora, where domain
// rotation between octaves destroyed the anisotropy the rays needed. Same mechanism, opposite sign:
// there the anisotropy was the feature, here the anisotropy is the lattice showing through.
float fbm(float x, float y, int octaves, std::uint32_t seed) noexcept
{
    // ~40 degrees: not a fraction of a right angle, so repeated application never returns to axis
    // alignment.
    constexpr float k_cos = 0.7654f;
    constexpr float k_sin = 0.6435f;

    float sum = 0.f;
    float amplitude = 0.5f;
    float total = 0.f;

    for(int i = 0; i < octaves; ++i) {
        sum += value_noise(x, y, seed + static_cast<std::uint32_t>(i) * 7919U) * amplitude;
        total += amplitude;

        const float rx = x * k_cos - y * k_sin;
        const float ry = x * k_sin + y * k_cos;

        x = rx * 2.03f + 11.7f;
        y = ry * 2.03f - 5.3f;
        amplitude *= 0.5f;
    }

    return total > 0.f ? sum / total : 0.f;
}

// How much cloud there is at this point, before any threshold - so the number is comparable within
// a tile but not between tiles, which is exactly what the caller needs.
//
// The boundary is not a detail. Tiles sit next to each other in one texture, and a cloud that is
// still solid at its own edge would both butt against its neighbour under bilinear filtering and
// end in a straight vertical line on screen. The rim mask is what makes the tile an island.
float tile_coverage(float u, float v, std::uint32_t seed) noexcept
{
    // -1..1 across the tile.
    const float x = u * 2.f - 1.f;
    const float y = v * 2.f - 1.f;

    const float radius = std::sqrt(x * x + y * y);

    // The rim mask. Flat at 1 across the middle so the cloud has a full-density core, and gone by
    // 0.96 rather than 1.0 so the outermost texels are certainly zero even after the mip chain has
    // averaged them with their neighbours.
    const float mask = 1.f - smoothstep01(std::clamp((radius - 0.60f) / 0.36f, 0.f, 1.f));
    if(mask <= 0.f) {
        // Below anything the field itself can reach, and deliberately not zero. The value returned
        // here shares a scale with the interior now that the threshold is a quantile over both, and
        // a zero would sit *above* the eroded values just inside the rim - which would put the cut
        // beneath it and light the tile's corners up.
        return -1.f;
    }

    // A domain warp: sampling the noise at coordinates that are themselves noisy is what turns
    // round lobes into ones that lean and curl, and it is much cheaper than another two octaves.
    const float warp = fbm(x * 1.8f, y * 1.8f, 2, seed + 991U);
    const float h = fbm(x * 2.8f + warp * 0.6f, y * 2.8f - warp * 0.5f, 5, seed);

    // The mask is *subtracted* from the height rather than multiplied into it, and that is the whole
    // difference between a cloud and a wisp.
    //
    // Multiplying scales the density down everywhere the mask is under one, which raises the
    // effective threshold continuously from the centre outwards - so the only texels that survive
    // are where the noise happens to be near its maximum, and the tile comes out as a ring of
    // shreds with no body anywhere. Measured on the first attempt: 10% of the tile covered and 0.1%
    // of it solid.
    //
    // Subtracting leaves the middle of the tile at its full height and only erodes near the rim,
    // which is what a coverage bias is supposed to do.
    //
    // Returned raw. Where the threshold goes is bake_tile's business, because it depends on this
    // tile's own distribution rather than on a number anyone can write down here.
    return h - (1.f - mask) * 0.50f;
}

// Box filter, one level to the next. Written out because there is no D3DX here to do it - see
// sky_math for why the SDK is headers only in this project.
//
// Mip levels matter more than they look like they should: the size slider goes down to half a
// degree, which minifies a 256-pixel tile onto some thirty screen pixels, and an unfiltered texture
// at that ratio sparkles.
void build_mip(const std::vector<std::uint32_t>& src, int src_w, int src_h, std::vector<std::uint32_t>& dst) noexcept
{
    const int w = (std::max)(1, src_w / 2);
    const int h = (std::max)(1, src_h / 2);

    dst.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

    for(int y = 0; y < h; ++y) {
        for(int x = 0; x < w; ++x) {
            const int x0 = (std::min)(x * 2, src_w - 1);
            const int x1 = (std::min)(x * 2 + 1, src_w - 1);
            const int y0 = (std::min)(y * 2, src_h - 1);
            const int y1 = (std::min)(y * 2 + 1, src_h - 1);

            const std::uint32_t p[4] { src[static_cast<std::size_t>(y0) * src_w + x0],
                src[static_cast<std::size_t>(y0) * src_w + x1],
                src[static_cast<std::size_t>(y1) * src_w + x0],
                src[static_cast<std::size_t>(y1) * src_w + x1] };

            std::uint32_t out = 0;
            for(int channel = 0; channel < 4; ++channel) {
                const int shift = channel * 8;
                unsigned int sum = 0;
                for(const std::uint32_t sample : p) {
                    sum += (sample >> shift) & 0xffU;
                }
                out |= ((sum + 2) / 4) << shift;
            }

            dst[static_cast<std::size_t>(y) * w + x] = out;
        }
    }
}

// One tile's worth of pixels, written into the full-atlas buffer at its grid position.
void bake_tile(std::vector<std::uint32_t>& pixels, int tile_index) noexcept
{
    const int size = g_tile_size;

    const int origin_x = (tile_index % g_tiles_x) * size;
    const int origin_y = (tile_index / g_tiles_x) * size;

    const auto seed = static_cast<std::uint32_t>(tile_index) * 2654435761U + 17U;

    // The field is needed a row above and below to take a gradient, so it is computed once into a
    // buffer rather than three times per texel.
    std::vector<float> height(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
    for(int y = 0; y < size; ++y) {
        for(int x = 0; x < size; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
            height[static_cast<std::size_t>(y) * size + x] = tile_coverage(u, v, seed);
        }
    }

    // The threshold is taken from this tile's own distribution rather than written down as a
    // constant, and that is a correction rather than a refinement.
    //
    // A fixed cut against fbm means each tile gets whatever coverage its seed happens to produce:
    // measured across four tiles, 8.6%, 19.9%, 27.7% and 8.8%. That is not four clouds, it is one
    // cloud and three smears, and no single constant fixes it because the thing that varies is the
    // field's own mean. Asking for a quantile instead states the intent directly - "about this much
    // of the tile is cloud" - and every tile then differs in shape, which is the only way they were
    // ever supposed to differ.
    constexpr float k_target_coverage = 0.42f;

    // Every fourth texel in each direction: sixteen times less to sort, and a quantile does not get
    // measurably better from the other fifteen sixteenths.
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(size / 4) * static_cast<std::size_t>(size / 4));
    for(int y = 0; y < size; y += 4) {
        for(int x = 0; x < size; x += 4) {
            samples.push_back(height[static_cast<std::size_t>(y) * size + x]);
        }
    }
    std::sort(samples.begin(), samples.end());

    const auto quantile = [&](float q) {
        const auto index = static_cast<std::size_t>(std::clamp(q, 0.f, 1.f) * static_cast<float>(samples.size() - 1));
        return samples[index];
    };

    const float cut = quantile(1.f - k_target_coverage);

    // The soft band is a fraction of the field's own spread, for the same reason: a fixed width
    // against a field whose range varies is a hard edge on one tile and a fog bank on another.
    const float band = (std::max)((quantile(0.97f) - cut) * 0.55f, 1e-3f);

    // Normalised into units of the band, and deliberately *not* clamped. The clamped version is the
    // opacity; this one is the surface, and the difference decides whether the sprite has one.
    //
    // Taking the gradient from the clamped density was the first attempt, and it produces a normal
    // map that is flat everywhere inside the cloud and violent along its outline - because a
    // saturated field has no gradient. Lit with that, a sprite is a flat cutout with a glowing rim.
    // Leaving the field unclamped keeps the underlying fbm's structure through the whole interior,
    // which is where the lumps have to come from.
    for(float& value : height) {
        value = (value - cut) / band;
    }

    const auto shape = [&](int x, int y) {
        return height[static_cast<std::size_t>(std::clamp(y, 0, size - 1)) * size + std::clamp(x, 0, size - 1)];
    };

    const auto density = [&](int x, int y) {
        return smoothstep01(std::clamp(shape(x, y), 0.f, 1.f));
    };

    for(int y = 0; y < size; ++y) {
        for(int x = 0; x < size; ++x) {
            // Central differences on the unclamped surface, and the opacity from the clamped one.
            //
            // The slope is in units of "bands per texel", which is what makes the multiplier below a
            // number with a meaning rather than a fitted constant: the band is the contrast the
            // threshold works over, so a slope of one is the field crossing the whole silhouette
            // transition in a single texel. Typical slopes are far under that, hence the scale.
            // Differenced across several texels rather than adjacent ones, which is a low-pass on
            // the slope.
            //
            // Every octave of an fbm contributes equally to its *derivative* - amplitude halves
            // while frequency doubles - so a one-texel difference is dominated by the finest octave
            // no matter how little it contributes to the shape. Lit from that, the sprite comes out
            // crinkled like foil instead of lumpy like a cloud. Widening the stencil averages the
            // fine octaves away and leaves the lobes, which are the things worth lighting.
            constexpr int k_span = 3;
            constexpr float k_relief = 1.4f;

            const float dx = shape(x + k_span, y) - shape(x - k_span, y);
            const float dy = shape(x, y + k_span) - shape(x, y - k_span);

            float nx = -dx * k_relief;
            float ny = -dy * k_relief;
            float nz = 1.f;

            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= length;
            ny /= length;
            nz /= length;

            const auto encode = [](float value) {
                return static_cast<std::uint32_t>(std::clamp(value * 0.5f + 0.5f, 0.f, 1.f) * 255.f + 0.5f);
            };

            const std::uint32_t r = encode(nx);
            const std::uint32_t g = encode(ny);
            const std::uint32_t b = encode(nz);
            const auto a = static_cast<std::uint32_t>(density(x, y) * 255.f + 0.5f);

            // D3DFMT_A8R8G8B8 is a 32-bit word, not a byte order: on a little-endian machine the
            // bytes land B, G, R, A, and packing the word is what makes that somebody else's
            // problem.
            pixels[static_cast<std::size_t>(origin_y + y) * atlas_width() + (origin_x + x)] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

bool upload(IDirect3DDevice9* device, std::vector<std::uint32_t>& pixels)
{
    // A full mip chain. 512 -> 1, which is nine levels and a third more memory than level zero
    // alone; the whole thing is a megabyte and a third.
    if(FAILED(device->CreateTexture(atlas_width(), atlas_height(), 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_texture, nullptr))
        || g_texture == nullptr) {
        TW_LOG_ERROR("sky_sprite_atlas: CreateTexture({}x{}) failed", atlas_width(), atlas_height());
        return false;
    }

    int width = atlas_width();
    int height = atlas_height();

    std::vector<std::uint32_t> level = std::move(pixels);
    std::vector<std::uint32_t> next;

    const DWORD levels = g_texture->GetLevelCount();
    for(DWORD i = 0; i < levels; ++i) {
        D3DLOCKED_RECT locked {};
        if(FAILED(g_texture->LockRect(i, &locked, nullptr, 0)) || locked.pBits == nullptr) {
            TW_LOG_ERROR("sky_sprite_atlas: LockRect(level {}) failed", static_cast<unsigned long>(i));
            return false;
        }

        // Pitch is in bytes and is not necessarily the row width - a driver is free to pad.
        auto* destination = static_cast<std::uint8_t*>(locked.pBits);
        for(int y = 0; y < height; ++y) {
            std::memcpy(destination + static_cast<std::size_t>(y) * static_cast<std::size_t>(locked.Pitch),
                level.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width),
                static_cast<std::size_t>(width) * sizeof(std::uint32_t));
        }

        g_texture->UnlockRect(i);

        if(i + 1 < levels) {
            build_mip(level, width, height, next);
            level.swap(next);
            width = (std::max)(1, width / 2);
            height = (std::max)(1, height / 2);
        }
    }

    return true;
}
} // namespace

namespace tw::skybox::sprite_atlas
{
IDirect3DTexture9* ensure(IDirect3DDevice9* device) noexcept
{
    if(device == nullptr) {
        return nullptr;
    }

    if(device != g_device) {
        // Nothing can be released through a device we no longer hold; the pointer is dropped, as
        // everywhere else in this module.
        g_texture = nullptr;
        g_failed = false;
        g_device = device;
    }

    // Before the failure flag, not after: there is nothing to fail at. A layer that paints its own
    // sprites asks for no texture and gets none, and the caller distinguishes this from a refusal by
    // asking untextured() first.
    if(g_source == source::none) {
        return nullptr;
    }

    if(g_texture != nullptr) {
        return g_texture;
    }

    if(g_failed) {
        return nullptr;
    }

    const auto started = std::chrono::steady_clock::now();

    std::vector<std::uint32_t> pixels;

    if(g_source == source::supplied) {
        // Already packed by set_tiles(). Copied rather than moved, and the state stays `supplied`, so
        // a device change can re-upload the same atlas without asking the script to bake it again.
        pixels = g_supplied;
    }
    else {
        pixels.assign(static_cast<std::size_t>(atlas_width()) * static_cast<std::size_t>(atlas_height()), 0U);
        for(int i = 0; i < g_tile_count; ++i) {
            bake_tile(pixels, i);
        }
    }

    if(!upload(device, pixels)) {
        if(g_texture != nullptr) {
            g_texture->Release();
            g_texture = nullptr;
        }
        g_failed = true;
        return nullptr;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    TW_LOG_INFO("sky_sprite_atlas: {} {} tiles into {}x{} in {} ms",
        g_source == source::supplied ? "uploaded" : "baked",
        g_tile_count,
        atlas_width(),
        atlas_height(),
        elapsed.count());

    return g_texture;
}

int tile_count() noexcept
{
    return g_tile_count;
}

int tile_size() noexcept
{
    return g_source == source::none ? 0 : g_tile_size;
}

grid_shape grid_for(int count) noexcept
{
    const int wanted = (std::max)(count, 1);
    const int columns = (std::max)(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(wanted)))));

    return { columns, (wanted + columns - 1) / columns };
}

bool untextured() noexcept
{
    return g_source == source::none;
}

void set_none() noexcept
{
    if(g_source == source::none) {
        return;
    }

    g_source = source::none;

    g_supplied.clear();
    g_supplied.shrink_to_fit();

    // One notional tile, so a generator asking how many there are gets an answer it can take a
    // modulus of. The grid it describes is never packed into anything - tile() short-circuits before
    // it would divide by the zero size below.
    g_tiles_x = 1;
    g_tiles_y = 1;
    g_tile_count = 1;
    g_tile_size = 0;

    release_device_resources();
}

void set_tiles(std::span<const image::layer> tiles) noexcept
{
    // Back to the built-in bake. Selecting a sky whose layer declares no fill has to undo whatever
    // the previous one supplied, or its clouds would keep somebody else's texture.
    if(tiles.empty()) {
        if(g_source == source::builtin) {
            return;
        }

        g_source = source::builtin;
        g_supplied.clear();
        g_supplied.shrink_to_fit();

        g_tile_size = k_builtin_tile_size;
        choose_grid(k_builtin_tiles);

        release_device_resources();

        return;
    }

    const int size = tiles.front().width;

    for(const image::layer& tile : tiles) {
        if(!tile.valid() || tile.width != size || tile.height != size) {
            TW_LOG_ERROR("sky_sprite_atlas: supplied tiles are not all {} square - keeping the previous atlas", size);
            return;
        }
    }

    g_tile_size = size;
    choose_grid(static_cast<int>(tiles.size()));

    g_supplied.assign(static_cast<std::size_t>(atlas_width()) * static_cast<std::size_t>(atlas_height()), 0U);

    std::vector<std::uint32_t> quantised;

    for(std::size_t i = 0; i < tiles.size(); ++i) {
        image::to_bgra(tiles[i], quantised);

        const int origin_x = (static_cast<int>(i) % g_tiles_x) * size;
        const int origin_y = (static_cast<int>(i) / g_tiles_x) * size;

        for(int y = 0; y < size; ++y) {
            std::copy_n(quantised.begin() + static_cast<std::ptrdiff_t>(y) * size,
                size,
                g_supplied.begin() + static_cast<std::ptrdiff_t>(origin_y + y) * atlas_width() + origin_x);
        }
    }

    g_source = source::supplied;

    // The texture on the device was built from the old tiles; dropping it is what makes the next
    // ensure() pick these up.
    release_device_resources();
}

tile_rect tile(int index) noexcept
{
    // The whole quad. Not a degenerate case of the grid below but a different answer to a different
    // question: with no atlas there is no cell to sit in and no neighbour to stay out of, and the
    // arithmetic below would in any case divide by an atlas zero texels wide.
    if(g_source == source::none) {
        return { 0.f, 0.f, 1.f, 1.f };
    }

    const int wrapped = ((index % g_tile_count) + g_tile_count) % g_tile_count;

    const int column = wrapped % g_tiles_x;
    const int row = wrapped / g_tiles_x;

    // Half a texel in from each side. Bilinear filtering reads two texels either way, so a quad
    // whose coordinates run to the exact tile boundary would pull in the neighbouring cloud along
    // its edge - faintly, but on every sprite, which is enough to see.
    const float inset_u = 0.5f / static_cast<float>(atlas_width());
    const float inset_v = 0.5f / static_cast<float>(atlas_height());

    const float span_u = 1.f / static_cast<float>(g_tiles_x);
    const float span_v = 1.f / static_cast<float>(g_tiles_y);

    const float u0 = static_cast<float>(column) * span_u;
    const float v0 = static_cast<float>(row) * span_v;

    return { u0 + inset_u, v0 + inset_v, u0 + span_u - inset_u, v0 + span_v - inset_v };
}

void release_device_resources() noexcept
{
    if(g_texture != nullptr) {
        g_texture->Release();
        g_texture = nullptr;
    }

    g_device = nullptr;
    g_failed = false;
}
} // namespace tw::skybox::sprite_atlas
