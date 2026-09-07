#include "pch.hxx"

#include "skybox/sky_image.hxx"

#include "plugin/diagnostics.hxx"

#include "libstb/stb_image.h"

namespace
{
float smoothstep01(float t) noexcept
{
    return t * t * (3.f - 2.f * t);
}

float hash01(int x, int y, std::uint32_t seed) noexcept
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(y) * 668265263U + seed;
    h = (h ^ (h >> 13)) * 1274126177U;

    return static_cast<float>((h ^ (h >> 16)) & 0xffffffU) / static_cast<float>(0x1000000U);
}

float value_noise(float x, float y, std::uint32_t seed) noexcept
{
    const auto fx = static_cast<float>(std::floor(x));
    const auto fy = static_cast<float>(std::floor(y));

    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);

    const float tx = smoothstep01(x - fx);
    const float ty = smoothstep01(y - fy);

    const float a = hash01(ix, iy, seed);
    const float b = hash01(ix + 1, iy, seed);
    const float c = hash01(ix, iy + 1, seed);
    const float d = hash01(ix + 1, iy + 1, seed);

    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// ~40 degrees per octave: not a fraction of a right angle, so repeated application never returns to
// axis alignment. See the note in sky_image.hxx for what happens without it.
constexpr float k_rot_cos = 0.7654f;
constexpr float k_rot_sin = 0.6435f;

float fbm_at(float x, float y, int octaves, std::uint32_t seed) noexcept
{
    float sum = 0.f;
    float amplitude = 0.5f;
    float total = 0.f;

    for(int i = 0; i < octaves; ++i) {
        sum += value_noise(x, y, seed + static_cast<std::uint32_t>(i) * 7919U) * amplitude;
        total += amplitude;

        const float rx = x * k_rot_cos - y * k_rot_sin;
        const float ry = x * k_rot_sin + y * k_rot_cos;

        x = rx * 2.03f + 11.7f;
        y = ry * 2.03f - 5.3f;
        amplitude *= 0.5f;
    }

    return total > 0.f ? sum / total : 0.f;
}

int clamp_channel(int channel) noexcept
{
    return std::clamp(channel, 0, 3);
}

// Bilinear sample in source texel coordinates, edge-clamped.
void sample(const tw::skybox::image::layer& source, float x, float y, std::array<float, 4>& out) noexcept
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);

    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);

    const float tx = x - fx;
    const float ty = y - fy;

    const float* p00 = source.clamped(x0, y0);
    const float* p10 = source.clamped(x0 + 1, y0);
    const float* p01 = source.clamped(x0, y0 + 1);
    const float* p11 = source.clamped(x0 + 1, y0 + 1);

    for(int c = 0; c < 4; ++c) {
        const float top = p00[c] + (p10[c] - p00[c]) * tx;
        const float bottom = p01[c] + (p11[c] - p01[c]) * tx;

        out[static_cast<std::size_t>(c)] = top + (bottom - top) * ty;
    }
}

float blend_channel(tw::skybox::image::blend mode, float dst, float src, float alpha) noexcept
{
    using tw::skybox::image::blend;

    switch(mode) {
        case blend::add:
            return dst + src * alpha;
        case blend::multiply:
            return dst * (1.f - alpha) + dst * src * alpha;
        case blend::screen:
            return dst + (1.f - dst) * src * alpha;
        case blend::max_of:
            return (std::max)(dst, src * alpha);
        case blend::replace:
            return dst + (src - dst) * alpha;
        default:
            // Straight-alpha "over". The source's own alpha decides coverage; `alpha` here is that
            // times the operation's opacity.
            return dst * (1.f - alpha) + src * alpha;
    }
}
} // namespace

namespace tw::skybox::image
{
layer make(int width, int height, std::array<float, 4> fill)
{
    layer out;

    if(width <= 0 || height <= 0) {
        return out;
    }

    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<std::size_t>(width) * height * 4);

    for(std::size_t i = 0; i < out.pixels.size(); i += 4) {
        out.pixels[i + 0] = fill[0];
        out.pixels[i + 1] = fill[1];
        out.pixels[i + 2] = fill[2];
        out.pixels[i + 3] = fill[3];
    }

    return out;
}

bool decode(std::span<const std::byte> bytes, layer& out)
{
    out = {};

    if(bytes.empty()) {
        return false;
    }

    int width = 0;
    int height = 0;
    int components = 0;

    // stbi_loadf gives linear float for an LDR source too - it applies the sRGB-ish transfer stb
    // uses - which is what compositing wants: blending in a non-linear space is the other way to get
    // banding, and it does not go away with more bits.
    float* pixels = stbi_loadf_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &components, 4);

    if(pixels == nullptr || width <= 0 || height <= 0) {
        if(pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);

    stbi_image_free(pixels);

    return true;
}

layer resize(const layer& source, int width, int height)
{
    layer out = make(width, height);

    if(!source.valid() || !out.valid()) {
        return out;
    }

    const float sx = static_cast<float>(source.width) / static_cast<float>(width);
    const float sy = static_cast<float>(source.height) / static_cast<float>(height);

    std::array<float, 4> texel {};

    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            // Sampling at texel centres, not corners: half-texel offsets are how a resize acquires a
            // shift that only shows up as a seam once the result is tiled.
            sample(source, (static_cast<float>(x) + 0.5f) * sx - 0.5f, (static_cast<float>(y) + 0.5f) * sy - 0.5f, texel);

            float* target = out.at(x, y);
            for(int c = 0; c < 4; ++c) {
                target[c] = texel[static_cast<std::size_t>(c)];
            }
        }
    }

    return out;
}

std::vector<layer> slice(const layer& sheet, int columns, int rows, int count, int size)
{
    std::vector<layer> out;

    if(!sheet.valid() || columns <= 0 || rows <= 0 || count <= 0 || size <= 0) {
        return out;
    }

    const int cell_width = sheet.width / columns;
    const int cell_height = sheet.height / rows;

    if(cell_width <= 0 || cell_height <= 0) {
        return out;
    }

    out.reserve(static_cast<std::size_t>(count));

    for(int i = 0; i < count; ++i) {
        const int origin_x = (i % columns) * cell_width;
        const int origin_y = (i / columns) * cell_height;

        // A cell past the last row of the grid. Reachable whenever the count is not a rectangle -
        // three tiles on a 2x2 grid - and answered with an empty cell rather than by refusing the
        // whole sheet, because the missing corner is the author's own layout and the other three are
        // what they asked for.
        if(origin_y + cell_height > sheet.height) {
            out.push_back(make(size, size));
            continue;
        }

        layer cell = make(cell_width, cell_height);

        for(int y = 0; y < cell_height; ++y) {
            std::copy_n(sheet.at(origin_x, origin_y + y), static_cast<std::size_t>(cell_width) * 4, cell.at(0, y));
        }

        out.push_back(cell_width == size && cell_height == size ? std::move(cell) : resize(cell, size, size));
    }

    return out;
}

void composite(layer& dst, const layer& src, const composite_options& options)
{
    if(!dst.valid() || !src.valid()) {
        return;
    }

    const float scale = std::fabs(options.scale) < 1e-6f ? 1e-6f : options.scale;

    const float cs = std::cos(-options.rotation);
    const float sn = std::sin(-options.rotation);

    // The source's centre lands at (x, y). Walking the destination and inverting the placement, so
    // every destination texel is written exactly once - the forward direction would leave holes
    // wherever the scale is above one.
    const float half_w = static_cast<float>(src.width) * 0.5f;
    const float half_h = static_cast<float>(src.height) * 0.5f;

    std::array<float, 4> texel {};

    for(int y = 0; y < dst.height; ++y) {
        for(int x = 0; x < dst.width; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - options.x;
            const float dy = (static_cast<float>(y) + 0.5f) - options.y;

            const float rx = (dx * cs - dy * sn) / scale;
            const float ry = (dx * sn + dy * cs) / scale;

            const float u = rx + half_w;
            const float v = ry + half_h;

            // Outside the source entirely: skipped rather than edge-clamped, or a placed image would
            // smear its border across the whole destination.
            if(u < 0.f || v < 0.f || u >= static_cast<float>(src.width) || v >= static_cast<float>(src.height)) {
                continue;
            }

            sample(src, u - 0.5f, v - 0.5f, texel);

            float* target = dst.at(x, y);

            const float coverage = options.mode == blend::replace ? options.opacity
                                                                  : std::clamp(texel[3], 0.f, 1.f) * options.opacity;

            for(int c = 0; c < 4; ++c) {
                if(!options.channels[static_cast<std::size_t>(c)]) {
                    continue;
                }

                // Alpha composites as coverage rather than as a colour, or an "over" would leave the
                // destination transparent wherever the source was.
                if(c == 3 && options.mode == blend::normal) {
                    target[c] = target[c] + (1.f - target[c]) * coverage;
                    continue;
                }

                target[c] = blend_channel(options.mode, target[c], texel[static_cast<std::size_t>(c)], coverage);
            }
        }
    }
}

void fbm(layer& target, const fbm_options& options)
{
    if(!target.valid()) {
        return;
    }

    const int channel = clamp_channel(options.channel);
    const int octaves = std::clamp(options.octaves, 1, 12);

    const auto width = static_cast<float>(target.width);
    const auto height = static_cast<float>(target.height);

    for(int y = 0; y < target.height; ++y) {
        for(int x = 0; x < target.width; ++x) {
            // Normalised to the layer, so the same options give the same look at any tile size.
            const float u = (static_cast<float>(x) + 0.5f) / width;
            const float v = (static_cast<float>(y) + 0.5f) / height;

            float value = 0.f;

            if(options.warp > 0.f) {
                // A second, coarser field displacing the domain of the first. This is what turns the
                // rounded blobs of plain fbm into something with folds.
                const float warp = fbm_at(u * options.warp_frequency, v * options.warp_frequency, 2, options.seed + 991U);

                value = fbm_at(u * options.frequency + warp * options.warp,
                    v * options.frequency - warp * options.warp * 0.83f,
                    octaves,
                    options.seed);
            }
            else {
                value = fbm_at(u * options.frequency, v * options.frequency, octaves, options.seed);
            }

            target.at(x, y)[channel] = value;
        }
    }
}

void radial(layer& target, const radial_options& options)
{
    if(!target.valid()) {
        return;
    }

    const int channel = clamp_channel(options.channel);
    const float feather = (std::max)(options.feather, 1e-3f);

    for(int y = 0; y < target.height; ++y) {
        for(int x = 0; x < target.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(target.width) * 2.f - 1.f;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(target.height) * 2.f - 1.f;

            const float radius = std::sqrt(u * u + v * v);
            const float t = std::clamp((radius - options.inner) / feather, 0.f, 1.f);

            // Subtracted, not multiplied. See the header.
            target.at(x, y)[channel] -= smoothstep01(t) * options.depth;
        }
    }
}

void normalize(layer& target, const normalize_options& options)
{
    if(!target.valid()) {
        return;
    }

    const int channel = clamp_channel(options.channel);

    // Every fourth texel in each direction: sixteen times less to sort, and a quantile does not get
    // meaningfully better from more samples than this.
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(target.width / 4 + 1) * static_cast<std::size_t>(target.height / 4 + 1));

    for(int y = 0; y < target.height; y += 4) {
        for(int x = 0; x < target.width; x += 4) {
            samples.push_back(target.at(x, y)[channel]);
        }
    }

    if(samples.empty()) {
        return;
    }

    std::sort(samples.begin(), samples.end());

    const auto quantile = [&samples](float q) {
        const auto index = static_cast<std::size_t>(std::clamp(q, 0.f, 1.f) * static_cast<float>(samples.size() - 1));
        return samples[index];
    };

    const float coverage = std::clamp(options.coverage, 0.01f, 0.99f);
    const float cut = quantile(1.f - coverage);
    const float band = (std::max)((quantile(0.97f) - cut) * (std::max)(options.band, 1e-3f), 1e-3f);

    for(int y = 0; y < target.height; ++y) {
        for(int x = 0; x < target.width; ++x) {
            float& value = target.at(x, y)[channel];
            value = (value - cut) / band;
        }
    }
}

void normals(layer& target, const layer& height, const normals_options& options)
{
    if(!target.valid() || !height.valid()) {
        return;
    }

    const int channel = clamp_channel(options.height_channel);
    const int span = std::clamp(options.span, 1, 32);

    const auto shape = [&height, channel](int x, int y) {
        return height.clamped(x, y)[channel];
    };

    for(int y = 0; y < target.height; ++y) {
        for(int x = 0; x < target.width; ++x) {
            // Central differences on the *unclamped* surface, differenced across several texels
            // rather than adjacent ones - a low-pass on the slope. See the header for both reasons.
            const float dx = shape(x + span, y) - shape(x - span, y);
            const float dy = shape(x, y + span) - shape(x, y - span);

            float nx = -dx * options.relief;
            float ny = -dy * options.relief;
            float nz = 1.f;

            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= length;
            ny /= length;
            nz /= length;

            float* texel = target.at(x, y);

            // Encoded to 0..1, the convention the cloud pixel shader decodes.
            texel[0] = nx * 0.5f + 0.5f;
            texel[1] = ny * 0.5f + 0.5f;
            texel[2] = nz * 0.5f + 0.5f;
        }
    }
}

void alpha_from_height(layer& target, const layer& height, const alpha_options& options)
{
    if(!target.valid() || !height.valid()) {
        return;
    }

    const int channel = clamp_channel(options.height_channel);
    const float softness = (std::max)(options.softness, 1e-3f);

    for(int y = 0; y < target.height; ++y) {
        for(int x = 0; x < target.width; ++x) {
            const float shape = height.clamped(x, y)[channel] / softness;

            target.at(x, y)[3] = smoothstep01(std::clamp(shape, 0.f, 1.f));
        }
    }
}

void to_bgra(const layer& source, std::vector<std::uint32_t>& out)
{
    out.clear();

    if(!source.valid()) {
        return;
    }

    out.resize(static_cast<std::size_t>(source.width) * source.height);

    const auto encode = [](float value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f);
    };

    for(int y = 0; y < source.height; ++y) {
        for(int x = 0; x < source.width; ++x) {
            const float* texel = source.at(x, y);

            out[static_cast<std::size_t>(y) * source.width + x]
                = (encode(texel[3]) << 24) | (encode(texel[0]) << 16) | (encode(texel[1]) << 8) | encode(texel[2]);
        }
    }
}
} // namespace tw::skybox::image
