#include "pch.hxx"

#include "skybox/sky_cubemap.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_paths.hxx"

#include "libstb/stb_image.h"
#include "libstb/stb_image_resize2.h"

namespace
{
// The loaders below are file-local, but they hand back the module's public result type.
using tw::skybox::cubemap_result;

constexpr int k_cross_columns = 4;
constexpr int k_cross_rows = 3;
constexpr std::size_t k_face_count = 6;

// Hard ceiling on what min_face_size can ask for, independent of what the device would allow.
// 4096px faces are already 400MB of cube map; anything past that is a typo, not an intent.
constexpr int k_max_face_size = 4096;

// Decoding a single image this large gets a warning before it is attempted. The number is not about
// total memory - it is about one *contiguous* block, which is the thing a 32-bit process next to a
// running game actually runs out of. Measured: a 4096x2048 panorama (32MB) decodes fine, an
// 8192x4096 one (128MB) does not.
constexpr std::size_t k_large_decode_warning_mb = 64;

// D3DPOOL_MANAGED keeps a system-memory copy alongside the video one, so a cube map costs its size
// twice in a process that has only ~2GB of address space to begin with. Past this, say so.
constexpr std::size_t k_large_cube_warning_mb = 128;

// Floor for the halving retry in create_cube. Below this the sky is worse than the sphere it
// replaced, and failing outright is the more honest outcome.
constexpr int k_min_fallback_face_size = 256;

struct cross_tile {
    int column;
    int row;
};

// Indexed by D3DCUBEMAP_FACES, whose order is fixed by the D3D9 header: +X, -X, +Y, -Y, +Z, -Z.
// The tile positions are the standard horizontal-cross unfolding; the per-face pixel orientation
// D3D expects (+X reads u towards -Z and v towards -Y, and so on) is the same orientation a cross
// image already stores, so each tile is copied straight across with no flip or rotate.
//
// If a particular set of art turns out to be laid out differently - vertical cross, or a cross
// authored for OpenGL's flipped Y - this table plus a per-face flip flag is the single place that
// has to change.
constexpr std::array<cross_tile, k_face_count> k_cross_tiles { {
    { 2, 1 }, // D3DCUBEMAP_FACE_POSITIVE_X
    { 0, 1 }, // D3DCUBEMAP_FACE_NEGATIVE_X
    { 1, 0 }, // D3DCUBEMAP_FACE_POSITIVE_Y
    { 1, 2 }, // D3DCUBEMAP_FACE_NEGATIVE_Y
    { 1, 1 }, // D3DCUBEMAP_FACE_POSITIVE_Z
    { 3, 1 }, // D3DCUBEMAP_FACE_NEGATIVE_Z
} };

// Accepted file stems for a directory of six faces, in D3DCUBEMAP_FACES order. Three spellings each
// because every tool that exports cube maps picks a different one and none of them is wrong.
constexpr std::array<std::array<std::string_view, 3>, k_face_count> k_face_stems { {
    { "posx", "px", "right" },
    { "negx", "nx", "left" },
    { "posy", "py", "top" },
    { "negy", "ny", "bottom" },
    { "posz", "pz", "front" },
    { "negz", "nz", "back" },
} };

// Owning wrapper around an stb_image allocation, so every early return below stops leaking on its
// own instead of needing a free() on each path.
class decoded_image {
public:
    decoded_image() = default;

    decoded_image(const decoded_image&) = delete;
    decoded_image& operator=(const decoded_image&) = delete;

    decoded_image(decoded_image&& other) noexcept
        : m_pixels(std::exchange(other.m_pixels, nullptr)), m_width(std::exchange(other.m_width, 0)),
          m_height(std::exchange(other.m_height, 0))
    {
    }

    decoded_image& operator=(decoded_image&& other) noexcept
    {
        if(this != &other) {
            reset();
            m_pixels = std::exchange(other.m_pixels, nullptr);
            m_width = std::exchange(other.m_width, 0);
            m_height = std::exchange(other.m_height, 0);
        }
        return *this;
    }

    ~decoded_image()
    {
        reset();
    }

    void adopt(stbi_uc* pixels, int width, int height) noexcept
    {
        reset();
        m_pixels = pixels;
        m_width = width;
        m_height = height;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return m_pixels != nullptr && m_width > 0 && m_height > 0;
    }

    [[nodiscard]] const stbi_uc* pixels() const noexcept
    {
        return m_pixels;
    }

    [[nodiscard]] int width() const noexcept
    {
        return m_width;
    }

    [[nodiscard]] int height() const noexcept
    {
        return m_height;
    }

private:
    void reset() noexcept
    {
        if(m_pixels != nullptr) {
            stbi_image_free(m_pixels);
            m_pixels = nullptr;
        }
        m_width = 0;
        m_height = 0;
    }

    stbi_uc* m_pixels = nullptr;
    int m_width = 0;
    int m_height = 0;
};

// A square region of RGBA pixels inside some decoded image. Both source layouts (one cross, or six
// separate files) reduce to six of these, which is what keeps the upload path single.
struct face_rect {
    const stbi_uc* pixels;
    int stride_pixels; // full width of the image `pixels` belongs to
    int x;
    int y;
    int size;
};

// Radiance .hdr, tone mapped ourselves instead of letting stb clip it (see
// cubemap_source::hdr_exposure).
//
// The conversion runs in place, over the float buffer stb just handed back: four output bytes per
// twelve input, so the write cursor is always behind the read cursor. That matters more than it
// looks - this is a 32-bit process, and the float buffer for an 8K panorama is already 400MB. stb's
// own path would allocate a second buffer on top of that before freeing the first.
decoded_image tone_map_hdr(std::span<const std::byte> bytes, std::string_view what, float exposure)
{
    decoded_image out;

    int width = 0;
    int height = 0;
    int components = 0;
    float* hdr = stbi_loadf_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &components, 3);

    if(hdr == nullptr || width <= 0 || height <= 0) {
        TW_LOG_ERROR("sky_cubemap: could not decode '{}' as HDR (a {}x{} panorama needs ~{} MB of float buffer, and this is a 32-bit "
                     "process)",
            what,
            width,
            height,
            (static_cast<std::size_t>(width) * height * 3 * sizeof(float)) / (1024 * 1024));
        if(hdr != nullptr) {
            stbi_image_free(hdr);
        }
        return out;
    }

    if(!std::isfinite(exposure) || exposure <= 0.f) {
        exposure = 1.f;
    }

    auto* ldr = reinterpret_cast<stbi_uc*>(hdr);
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;

    for(std::size_t i = 0; i < pixel_count; ++i) {
        // Read all three before writing any: the output for pixel i overlaps the input for pixels
        // below it, never for i itself.
        const float r = hdr[i * 3 + 0] * exposure;
        const float g = hdr[i * 3 + 1] * exposure;
        const float b = hdr[i * 3 + 2] * exposure;

        const auto encode = [](float value) noexcept {
            const float rolled = value / (1.f + value); // Reinhard: the sun compresses, never clips
            const float gamma = std::pow(std::clamp(rolled, 0.f, 1.f), 1.f / 2.2f);
            return static_cast<stbi_uc>(gamma * 255.f + 0.5f);
        };

        ldr[i * 4 + 0] = encode(r);
        ldr[i * 4 + 1] = encode(g);
        ldr[i * 4 + 2] = encode(b);
        ldr[i * 4 + 3] = 255;
    }

    TW_LOG_INFO("sky_cubemap: '{}' is a {}x{} Radiance HDR, tone mapped at exposure {}", what, width, height, exposure);

    out.adopt(ldr, width, height);
    return out;
}

decoded_image decode_bytes(std::span<const std::byte> bytes, std::string_view what, float hdr_exposure)
{
    decoded_image out;

    if(bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        TW_LOG_ERROR("sky_cubemap: '{}' is empty or too large to decode", what);
        return out;
    }

    if(stbi_is_hdr_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size())) != 0) {
        return tone_map_hdr(bytes, what, hdr_exposure);
    }

    // Header first. stb only reports "outofmem" after it has already tried and failed, by which
    // point the interesting number - how much it was going to need - is gone. Reading the header is
    // a few bytes and turns a bare failure into an actionable one.
    int width = 0;
    int height = 0;
    int components = 0;

    if(stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &components)
        == 0) {
        TW_LOG_ERROR("sky_cubemap: '{}' is not an image stb can read ({})", what, stbi_failure_reason());
        return out;
    }

    const std::size_t decoded_megabytes = (static_cast<std::size_t>(width) * height * 4) / (1024 * 1024);

    if(decoded_megabytes >= k_large_decode_warning_mb) {
        TW_LOG_WARNING("sky_cubemap: '{}' is {}x{} - decoding it needs one contiguous {} MB block, plus whatever the decoder itself "
                       "wants. In a 32-bit process next to a running game that is not a safe bet; six separate face files decode one "
                       "at a time and avoid it entirely (see skybox-replacer.md)",
            what,
            width,
            height,
            decoded_megabytes);
    }

    stbi_uc* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &components, STBI_rgb_alpha);

    if(pixels == nullptr || width <= 0 || height <= 0) {
        TW_LOG_ERROR("sky_cubemap: could not decode '{}' ({}) - {}x{} needs {} MB contiguous",
            what,
            stbi_failure_reason(),
            width,
            height,
            decoded_megabytes);
        if(pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return out;
    }

    out.adopt(pixels, width, height);
    return out;
}

std::vector<std::byte> read_file(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if(ec || size == 0 || size > static_cast<std::uintmax_t>(INT_MAX)) {
        return {};
    }

    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return {};
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if(!file) {
        return {};
    }

    return bytes;
}

// Relative paths resolve against the DLL's own directory, not the process working directory - the
// game's CWD is engine/, which is not where anyone would think to put their skyboxes.
// Case-insensitive ASCII compare; the stems and extensions above are all ASCII by construction.
bool equals_ignore_case(std::string_view a, std::string_view b) noexcept
{
    if(a.size() != b.size()) {
        return false;
    }

    for(std::size_t i = 0; i < a.size(); ++i) {
        if(std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

// Walks the directory once and picks out whichever of the accepted spellings is there. Doing it by
// scan rather than by probing name x extension combinations means the extension list never has to
// be exhaustive - stb decodes more formats than anyone would think to enumerate.
bool collect_face_files(const std::filesystem::path& directory, std::array<std::filesystem::path, k_face_count>& out)
{
    std::error_code ec;
    for(const auto& entry : std::filesystem::directory_iterator { directory, ec }) {
        if(!entry.is_regular_file()) {
            continue;
        }

        const std::string stem = entry.path().stem().string();

        for(std::size_t face = 0; face < k_face_count; ++face) {
            if(!out[face].empty()) {
                continue;
            }

            for(const std::string_view accepted : k_face_stems[face]) {
                if(equals_ignore_case(stem, accepted)) {
                    out[face] = entry.path();
                    break;
                }
            }
        }
    }

    if(ec) {
        TW_LOG_ERROR("sky_cubemap: cannot enumerate '{}'", directory.string());
        return false;
    }

    for(std::size_t face = 0; face < k_face_count; ++face) {
        if(out[face].empty()) {
            TW_LOG_ERROR("sky_cubemap: '{}' has no image for face {} (expected one of {}/{}/{})",
                directory.string(),
                face,
                k_face_stems[face][0],
                k_face_stems[face][1],
                k_face_stems[face][2]);
            return false;
        }
    }

    return true;
}

// stb hands back straight RGBA; D3DFMT_A8R8G8B8 is BGRA in memory. One row at a time so the
// destination's pitch (which the driver is free to pad) is honoured.
void write_bgra(const stbi_uc* source, int source_stride_pixels, int size, const D3DLOCKED_RECT& locked)
{
    for(int y = 0; y < size; ++y) {
        const stbi_uc* src_row = source + static_cast<std::size_t>(y) * source_stride_pixels * 4;
        auto* dst_row = static_cast<std::uint8_t*>(locked.pBits) + static_cast<std::size_t>(y) * locked.Pitch;

        for(int x = 0; x < size; ++x) {
            dst_row[x * 4 + 0] = src_row[x * 4 + 2]; // B
            dst_row[x * 4 + 1] = src_row[x * 4 + 1]; // G
            dst_row[x * 4 + 2] = src_row[x * 4 + 0]; // R
            dst_row[x * 4 + 3] = src_row[x * 4 + 3]; // A
        }
    }
}

bool upload_face(IDirect3DCubeTexture9* cube, std::size_t face, const face_rect& src, int target_size, std::vector<stbi_uc>& scratch)
{
    const stbi_uc* tile = src.pixels + (static_cast<std::size_t>(src.y) * src.stride_pixels + src.x) * 4;

    int source_stride_pixels = src.stride_pixels;

    if(src.size != target_size) {
        scratch.resize(static_cast<std::size_t>(target_size) * target_size * 4);

        // sRGB-aware, because this is colour art and resampling it in gamma space darkens the
        // gradients. Edge mode is stb's default CLAMP, which is what a cube face wants - a face
        // must not bleed pixels in from its neighbours in the cross.
        if(stbir_resize_uint8_srgb(
               tile, src.size, src.size, src.stride_pixels * 4, scratch.data(), target_size, target_size, target_size * 4, STBIR_RGBA)
            == nullptr) {
            TW_LOG_ERROR("sky_cubemap: resample of face {} to {}px failed", face, target_size);
            return false;
        }

        tile = scratch.data();
        source_stride_pixels = target_size;
    }

    D3DLOCKED_RECT locked {};
    const HRESULT hr = cube->LockRect(static_cast<D3DCUBEMAP_FACES>(face), 0, &locked, nullptr, 0);
    if(FAILED(hr) || locked.pBits == nullptr) {
        TW_LOG_ERROR("sky_cubemap: LockRect failed on face {}, hr=0x{:08X}", face, static_cast<unsigned long>(hr));
        return false;
    }

    write_bgra(tile, source_stride_pixels, target_size, locked);
    cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face), 0);

    return true;
}

// Direction of the cube-face texel at (s, t), both in [-1, 1], for each D3DCUBEMAP_FACES entry.
// These are the standard D3D9 face orientations - the same ones the cross table above relies on,
// written out here because projecting a panorama needs them numerically rather than as a copy.
D3DXVECTOR3 face_direction(std::size_t face, float s, float t) noexcept
{
    switch(face) {
        case 0:
            return { 1.f, -t, -s };  // +X
        case 1:
            return { -1.f, -t, s };  // -X
        case 2:
            return { s, 1.f, t };    // +Y
        case 3:
            return { s, -1.f, -t };  // -Y
        case 4:
            return { s, -t, 1.f };   // +Z
        default:
            return { -s, -t, -1.f }; // -Z
    }
}

// Bilinear tap into an equirectangular panorama. Wraps horizontally (longitude is periodic) and
// clamps vertically (latitude is not).
void sample_equirect(const stbi_uc* pixels, int width, int height, float u, float v, stbi_uc* out) noexcept
{
    const float x = u * static_cast<float>(width) - 0.5f;
    const float y = v * static_cast<float>(height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    const auto wrap_x = [width](int value) noexcept {
        value %= width;
        return value < 0 ? value + width : value;
    };
    const auto clamp_y = [height](int value) noexcept {
        return std::clamp(value, 0, height - 1);
    };

    const int xi[2] { wrap_x(x0), wrap_x(x0 + 1) };
    const int yi[2] { clamp_y(y0), clamp_y(y0 + 1) };

    for(int channel = 0; channel < 4; ++channel) {
        const float c00 = pixels[(static_cast<std::size_t>(yi[0]) * width + xi[0]) * 4 + channel];
        const float c10 = pixels[(static_cast<std::size_t>(yi[0]) * width + xi[1]) * 4 + channel];
        const float c01 = pixels[(static_cast<std::size_t>(yi[1]) * width + xi[0]) * 4 + channel];
        const float c11 = pixels[(static_cast<std::size_t>(yi[1]) * width + xi[1]) * 4 + channel];

        const float top = c00 + (c10 - c00) * fx;
        const float bottom = c01 + (c11 - c01) * fx;

        out[channel] = static_cast<stbi_uc>(std::clamp(top + (bottom - top) * fy, 0.f, 255.f) + 0.5f);
    }
}

// Projects one cube face out of an equirectangular panorama, into a tight RGBA buffer.
void project_face(const decoded_image& panorama, std::size_t face, int face_size, std::vector<stbi_uc>& out)
{
    out.resize(static_cast<std::size_t>(face_size) * face_size * 4);

    const float inv_half = 2.f / static_cast<float>(face_size);

    for(int y = 0; y < face_size; ++y) {
        const float t = (static_cast<float>(y) + 0.5f) * inv_half - 1.f;

        for(int x = 0; x < face_size; ++x) {
            const float s = (static_cast<float>(x) + 0.5f) * inv_half - 1.f;

            const D3DXVECTOR3 dir = face_direction(face, s, t);
            const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

            // atan2(x, z) puts longitude 0 - the horizontal centre of the panorama - straight down
            // +Z, which is the convention every equirectangular sky is authored to. Latitude runs
            // from +90 at the top row to -90 at the bottom, hence the flip on v.
            const float longitude = std::atan2(dir.x, dir.z);
            const float latitude = std::asin(std::clamp(dir.y / length, -1.f, 1.f));

            const float u = 0.5f + longitude / (2.f * 3.14159265f);
            const float v = 0.5f - latitude / 3.14159265f;

            sample_equirect(panorama.pixels(),
                panorama.width(),
                panorama.height(),
                u,
                v,
                out.data() + (static_cast<std::size_t>(y) * face_size + x) * 4);
        }
    }
}

// Largest cube-map edge the device admits to supporting. Returns 0 when the caps say nothing
// useful, which callers treat as "no ceiling of our own to enforce".
int device_max_face_size(IDirect3DDevice9* device) noexcept
{
    D3DCAPS9 caps {};
    if(FAILED(device->GetDeviceCaps(&caps))) {
        return 0;
    }

    return static_cast<int>(caps.MaxTextureWidth);
}

std::size_t cube_megabytes(int face_size) noexcept
{
    return (static_cast<std::size_t>(face_size) * face_size * 4 * k_face_count) / (1024 * 1024);
}

// Creates the cube texture and nothing else, so callers that cannot afford to hold six decoded
// faces at once can create it up front and fill it one face at a time.
//
// `face_size` is in/out: on an out-of-memory refusal the request is halved and retried, and the
// caller is told what it actually got. A 2GB address space shared with a running game cannot promise
// 96MB on demand, so a sky one step softer than asked for beats no sky at all. Only E_OUTOFMEMORY is
// retried - every other HRESULT means something shrinking will not fix.
IDirect3DCubeTexture9* create_cube(IDirect3DDevice9* device, int& face_size, std::string_view what)
{
    const int device_limit = device_max_face_size(device);
    if(device_limit > 0 && face_size > device_limit) {
        TW_LOG_ERROR("sky_cubemap: '{}' wants {}px faces but the device caps out at {}px", what, face_size, device_limit);
        return nullptr;
    }

    const int requested = face_size;

    if(cube_megabytes(requested) >= k_large_cube_warning_mb) {
        TW_LOG_WARNING("sky_cubemap: '{}' builds a {} MB cube map, and D3DPOOL_MANAGED keeps a system-memory copy as well - budget "
                       "roughly double that in a 32-bit process",
            what,
            cube_megabytes(requested));
    }

    for(int size = requested; size >= k_min_fallback_face_size; size /= 2) {
        IDirect3DCubeTexture9* cube = nullptr;
        const HRESULT hr = device->CreateCubeTexture(static_cast<UINT>(size), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, nullptr);

        if(SUCCEEDED(hr) && cube != nullptr) {
            if(size != requested) {
                TW_LOG_WARNING("sky_cubemap: '{}' fell back to {}px faces ({} MB) - {}px would not fit. This is usually address space, "
                               "not the GPU: it happens right after the game rebuilds its D3D9 device, when a contiguous block that "
                               "size is hardest to find. A smaller source, or min_face_size, makes it deterministic",
                    what,
                    size,
                    cube_megabytes(size),
                    requested);
            }
            face_size = size;
            return cube;
        }

        TW_LOG_ERROR(
            "sky_cubemap: CreateCubeTexture({}) failed, hr=0x{:08X} ({} MB)", size, static_cast<unsigned long>(hr), cube_megabytes(size));

        if(hr != E_OUTOFMEMORY) {
            break;
        }
    }

    return nullptr;
}

cubemap_result build(IDirect3DDevice9* device, std::span<const face_rect> faces, int requested_size, std::string_view what)
{
    int target_size = requested_size;
    IDirect3DCubeTexture9* cube = create_cube(device, target_size, what);
    if(cube == nullptr) {
        return {};
    }

    std::vector<stbi_uc> scratch;

    for(std::size_t face = 0; face < k_face_count; ++face) {
        if(!upload_face(cube, face, faces[face], target_size, scratch)) {
            cube->Release();
            return {};
        }
    }

    const int source_size = faces[0].size;

    if(source_size == target_size) {
        TW_LOG_INFO("sky_cubemap: '{}' -> cube texture, {}px faces, {} MB", what, target_size, cube_megabytes(target_size));
    }
    else {
        TW_LOG_INFO("sky_cubemap: '{}' -> cube texture, {}px faces upscaled to {}px, {} MB",
            what,
            source_size,
            target_size,
            cube_megabytes(target_size));
    }

    return cubemap_result { cube, requested_size, target_size };
}

int clamp_target_size(int source_size, int min_face_size, std::string_view what)
{
    if(min_face_size <= source_size) {
        return source_size;
    }

    if(min_face_size > k_max_face_size) {
        TW_LOG_WARNING("sky_cubemap: min_face_size={} is past the {}px ceiling, clamping", min_face_size, k_max_face_size);
        min_face_size = k_max_face_size;
    }

    if(min_face_size <= source_size) {
        return source_size;
    }

    TW_LOG_INFO("sky_cubemap: '{}' has {}px faces, upscaling to {}px", what, source_size, min_face_size);

    return min_face_size;
}

// A 2:1 image is an equirectangular panorama. That ratio is unambiguous here: a horizontal cross is
// 4:3, and a single face is 1:1.
//
// Supporting these is what makes high-resolution skies actually obtainable. Ready-made cube maps at
// 2048px per face are rare and awkward (an 8192x6144 cross); equirectangular panoramas at 4K, 8K and
// 16K are everywhere and largely CC0, and one 8192x4096 panorama carries 2048px of genuine detail
// per face. stb_image also converts .hdr on the way in, so radiance maps work too - naively tone
// mapped, but they work.
cubemap_result from_equirect(IDirect3DDevice9* device, const decoded_image& panorama, std::string_view what)
{
    // One face spans 90 degrees, and the panorama spends a quarter of its width on each - so
    // width/4 is exactly the detail the source can supply, with no invention and no waste.
    int face_size = panorama.width() / 4;

    const int device_limit = device_max_face_size(device);
    const int ceiling = device_limit > 0 ? std::min(device_limit, k_max_face_size) : k_max_face_size;

    if(face_size > ceiling) {
        TW_LOG_INFO("sky_cubemap: '{}' could give {}px faces, capping at {}px", what, face_size, ceiling);
        face_size = ceiling;
    }

    if(face_size < 16) {
        TW_LOG_ERROR("sky_cubemap: '{}' is {}x{} - too small to project", what, panorama.width(), panorama.height());
        return {};
    }

    TW_LOG_INFO("sky_cubemap: '{}' is a {}x{} equirectangular panorama, projecting to {}px faces",
        what,
        panorama.width(),
        panorama.height(),
        face_size);

    // Through create_cube rather than CreateCubeTexture directly, so the projection path gets the
    // same out-of-memory halving as everything else - and, because the texture is created first,
    // each face is projected straight at whatever size actually got allocated. Projecting large and
    // resampling down would cost time and sharpness for nothing.
    const int requested = face_size;
    IDirect3DCubeTexture9* cube = create_cube(device, face_size, what);
    if(cube == nullptr) {
        return {};
    }

    std::vector<stbi_uc> projected;
    std::vector<stbi_uc> unused;

    for(std::size_t face = 0; face < k_face_count; ++face) {
        project_face(panorama, face, face_size, projected);

        const face_rect src { projected.data(), face_size, 0, 0, face_size };

        // Already at the target size, so upload_face takes its direct-copy path and never touches
        // the scratch buffer.
        if(!upload_face(cube, face, src, face_size, unused)) {
            cube->Release();
            return {};
        }
    }

    TW_LOG_INFO("sky_cubemap: '{}' -> cube texture, {}px faces, {} MB", what, face_size, cube_megabytes(face_size));

    return cubemap_result { cube, requested, face_size };
}

// Single-image entry point: decides between an equirectangular panorama and a horizontal cross by
// aspect ratio, since the two are never ambiguous.
cubemap_result from_single_image(IDirect3DDevice9* device, const decoded_image& image, int min_face_size, std::string_view what)
{
    if(image.width() == image.height() * 2) {
        return from_equirect(device, image, what);
    }

    const int face_size = image.width() / k_cross_columns;

    if(image.width() % k_cross_columns != 0 || image.height() % k_cross_rows != 0 || face_size != image.height() / k_cross_rows
        || face_size <= 0) {
        TW_LOG_ERROR("sky_cubemap: '{}' is {}x{}, which is not a 4x3 cross of square tiles", what, image.width(), image.height());
        return {};
    }

    std::array<face_rect, k_face_count> faces {};
    for(std::size_t face = 0; face < k_face_count; ++face) {
        faces[face] = face_rect {
            image.pixels(), image.width(), k_cross_tiles[face].column * face_size, k_cross_tiles[face].row * face_size, face_size
        };
    }

    return build(device, faces, clamp_target_size(face_size, min_face_size, what), what);
}

cubemap_result from_directory(IDirect3DDevice9* device, const std::filesystem::path& directory, int min_face_size, float hdr_exposure)
{
    const std::string what = directory.string();

    std::array<std::filesystem::path, k_face_count> face_files {};
    if(!collect_face_files(directory, face_files)) {
        return {};
    }

    // Two passes, and the split is the whole reason six separate faces beat one big image.
    //
    // Pass one reads the files and asks stb only for their headers, so the geometry can be
    // validated and the cube texture created without a single pixel being decoded. Pass two then
    // decodes exactly one face at a time and lets it go before touching the next.
    //
    // Peak decoded memory is therefore one face, not six: 16MB for 2048px faces where holding all
    // six would be 100MB, in a 32-bit process where a 128MB contiguous block has already been
    // measured to fail. The compressed bytes kept across both passes are a few megabytes each and
    // are not what runs the process out of address space.
    std::array<std::vector<std::byte>, k_face_count> file_bytes {};
    int face_size = 0;

    for(std::size_t face = 0; face < k_face_count; ++face) {
        file_bytes[face] = read_file(face_files[face]);
        if(file_bytes[face].empty()) {
            TW_LOG_ERROR("sky_cubemap: could not read '{}'", face_files[face].string());
            return {};
        }

        int width = 0;
        int height = 0;
        int components = 0;
        if(stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(file_bytes[face].data()),
               static_cast<int>(file_bytes[face].size()),
               &width,
               &height,
               &components)
            == 0) {
            TW_LOG_ERROR("sky_cubemap: '{}' is not an image stb can read ({})", face_files[face].string(), stbi_failure_reason());
            return {};
        }

        if(width != height) {
            TW_LOG_ERROR("sky_cubemap: face image '{}' is {}x{}, not square", face_files[face].string(), width, height);
            return {};
        }

        if(face == 0) {
            face_size = width;
        }
        else if(width != face_size) {
            TW_LOG_ERROR("sky_cubemap: face image '{}' is {}px but the first face is {}px - all six must match",
                face_files[face].string(),
                width,
                face_size);
            return {};
        }
    }

    int target_size = clamp_target_size(face_size, min_face_size, what);
    const int requested_size = target_size;

    IDirect3DCubeTexture9* cube = create_cube(device, target_size, what);
    if(cube == nullptr) {
        return {};
    }

    std::vector<stbi_uc> scratch;

    for(std::size_t face = 0; face < k_face_count; ++face) {
        const decoded_image image = decode_bytes(file_bytes[face], face_files[face].string(), hdr_exposure);

        // The compressed copy has done its job; drop it before the next face is decoded.
        file_bytes[face] = {};

        if(!image.valid()) {
            cube->Release();
            return {};
        }

        const face_rect src { image.pixels(), face_size, 0, 0, face_size };
        if(!upload_face(cube, face, src, target_size, scratch)) {
            cube->Release();
            return {};
        }
    }

    if(face_size == target_size) {
        TW_LOG_INFO("sky_cubemap: '{}' -> cube texture, {}px faces, {} MB", what, target_size, cube_megabytes(target_size));
    }
    else {
        TW_LOG_INFO("sky_cubemap: '{}' -> cube texture, {}px faces upscaled to {}px, {} MB",
            what,
            face_size,
            target_size,
            cube_megabytes(target_size));
    }

    return cubemap_result { cube, requested_size, target_size };
}
} // namespace

namespace tw::skybox
{
std::span<const std::array<std::string_view, 3>> face_stems() noexcept
{
    return k_face_stems;
}

cubemap_result create_cubemap(IDirect3DDevice9* device, const cubemap_source& source) noexcept
{
    if(device == nullptr) {
        return {};
    }

    if(!source.file_path.empty()) {
        const std::filesystem::path path = resolve_source_path(source.file_path);
        if(path.empty()) {
            return {};
        }

        std::error_code ec;
        if(std::filesystem::is_directory(path, ec)) {
            return from_directory(device, path, source.min_face_size, source.hdr_exposure);
        }

        if(!std::filesystem::is_regular_file(path, ec)) {
            TW_LOG_ERROR("sky_cubemap: '{}' is neither a file nor a directory", path.string());
            return {};
        }

        const decoded_image image = decode_bytes(read_file(path), path.string(), source.hdr_exposure);
        if(!image.valid()) {
            return {};
        }

        return from_single_image(device, image, source.min_face_size, path.string());
    }

    const auto res = tw::resource::get_resource(tw::resource::type::texture, source.resource_key);
    if(!res) {
        TW_LOG_ERROR("sky_cubemap: packed resource '{}' is missing", source.resource_key);
        return {};
    }

    const decoded_image image = decode_bytes(res->bytes, source.resource_key, source.hdr_exposure);
    if(!image.valid()) {
        return {};
    }

    return from_single_image(device, image, source.min_face_size, source.resource_key);
}
} // namespace tw::skybox
