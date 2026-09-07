#include "pch.hxx"

#include "skybox/sky_texture.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
constexpr std::uint32_t k_magic = 0x20534444U; // "DDS "

// DDS_PIXELFORMAT flags, from the format documentation rather than from a header - d3d9.h does not
// declare them and pulling in dxgiformat.h for four constants would be worse.
constexpr std::uint32_t k_pf_alphapixels = 0x1;
constexpr std::uint32_t k_pf_alpha = 0x2;
constexpr std::uint32_t k_pf_fourcc = 0x4;
constexpr std::uint32_t k_pf_rgb = 0x40;
constexpr std::uint32_t k_pf_luminance = 0x20000;

constexpr std::uint32_t k_caps2_cubemap = 0x200;
constexpr std::uint32_t k_caps2_volume = 0x200000;

constexpr std::uint32_t k_caps2_face_mask = 0xFC00; // all six DDSCAPS2_CUBEMAP_POSITIVEX..NEGATIVEZ

constexpr std::uint32_t k_header_flag_mipmapcount = 0x20000;

std::uint32_t four_cc(const char a, const char b, const char c, const char d) noexcept
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) | static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8
        | static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16
        | static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24;
}

#pragma pack(push, 1)
struct dds_pixelformat {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t four_cc;
    std::uint32_t rgb_bit_count;
    std::uint32_t red_mask;
    std::uint32_t green_mask;
    std::uint32_t blue_mask;
    std::uint32_t alpha_mask;
};

struct dds_header {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitch_or_linear_size;
    std::uint32_t depth;
    std::uint32_t mip_map_count;
    std::uint32_t reserved1[11];
    dds_pixelformat pixel_format;
    std::uint32_t caps;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};
#pragma pack(pop)

static_assert(sizeof(dds_header) == 124, "the DDS header is 124 bytes by definition");

// How many bytes one surface of `format` occupies at this size, and how the rows are laid out.
//
// Block-compressed formats count in 4x4 blocks, which is why this cannot simply be width times a
// bytes-per-pixel: a 2x2 DXT1 surface still occupies one whole 8-byte block.
struct surface_layout {
    std::size_t row_bytes {};
    std::size_t rows {};

    [[nodiscard]] std::size_t bytes() const noexcept
    {
        return row_bytes * rows;
    }
};

int block_bytes_of(D3DFORMAT format) noexcept
{
    switch(format) {
        case D3DFMT_DXT1:
            return 8;
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            return 16;
        default:
            return 0;
    }
}

int pixel_bytes_of(D3DFORMAT format) noexcept
{
    switch(format) {
        case D3DFMT_A8:
        case D3DFMT_L8:
            return 1;
        case D3DFMT_A8L8:
        case D3DFMT_R5G6B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_L16:
        case D3DFMT_R16F:
            return 2;
        case D3DFMT_R8G8B8:
            return 3;
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_R32F:
        case D3DFMT_G16R16F:
            return 4;
        case D3DFMT_A16B16G16R16:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_G32R32F:
            return 8;
        case D3DFMT_A32B32G32R32F:
            return 16;
        default:
            return 0;
    }
}

surface_layout layout_of(D3DFORMAT format, int width, int height) noexcept
{
    surface_layout out;

    const int block = block_bytes_of(format);
    if(block > 0) {
        out.row_bytes = static_cast<std::size_t>((std::max)(1, (width + 3) / 4)) * static_cast<std::size_t>(block);
        out.rows = static_cast<std::size_t>((std::max)(1, (height + 3) / 4));
        return out;
    }

    out.row_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(pixel_bytes_of(format));
    out.rows = static_cast<std::size_t>(height);

    return out;
}

// The DDS pixel format to a D3DFORMAT, or D3DFMT_UNKNOWN.
//
// Matched on masks rather than on bit count alone, because 32-bit RGB has two orderings in the wild
// and getting them the wrong way round swaps red and blue - which reads as an artistic choice rather
// than as a bug, and is therefore the kind of thing that survives review.
D3DFORMAT format_of(const dds_pixelformat& pf) noexcept
{
    if((pf.flags & k_pf_fourcc) != 0) {
        if(pf.four_cc == four_cc('D', 'X', 'T', '1')) {
            return D3DFMT_DXT1;
        }
        if(pf.four_cc == four_cc('D', 'X', 'T', '3')) {
            return D3DFMT_DXT3;
        }
        if(pf.four_cc == four_cc('D', 'X', 'T', '5')) {
            return D3DFMT_DXT5;
        }

        // The float formats are stored as the D3DFORMAT number itself where a FourCC would go. That
        // is not a convention this file invented; it is what every DDS writer does for them.
        switch(pf.four_cc) {
            case 111:
                return D3DFMT_R16F;
            case 112:
                return D3DFMT_G16R16F;
            case 113:
                return D3DFMT_A16B16G16R16F;
            case 114:
                return D3DFMT_R32F;
            case 115:
                return D3DFMT_G32R32F;
            case 116:
                return D3DFMT_A32B32G32R32F;
            case 36:
                return D3DFMT_A16B16G16R16;
            default:
                return D3DFMT_UNKNOWN;
        }
    }

    const bool has_alpha = (pf.flags & k_pf_alphapixels) != 0 && pf.alpha_mask != 0;

    if((pf.flags & k_pf_rgb) != 0) {
        if(pf.rgb_bit_count == 32) {
            if(pf.red_mask == 0x00ff0000 && pf.blue_mask == 0x000000ff) {
                return has_alpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
            }
            if(pf.red_mask == 0x000000ff && pf.blue_mask == 0x00ff0000) {
                return has_alpha ? D3DFMT_A8B8G8R8 : D3DFMT_X8B8G8R8;
            }
            if(pf.red_mask == 0x0000ffff && pf.green_mask == 0xffff0000) {
                return D3DFMT_G16R16;
            }
            return D3DFMT_UNKNOWN;
        }

        if(pf.rgb_bit_count == 24 && pf.red_mask == 0x00ff0000) {
            return D3DFMT_R8G8B8;
        }

        if(pf.rgb_bit_count == 16) {
            if(pf.red_mask == 0xf800 && pf.green_mask == 0x07e0) {
                return D3DFMT_R5G6B5;
            }
            if(pf.red_mask == 0x7c00) {
                return D3DFMT_A1R5G5B5;
            }
            if(pf.red_mask == 0x0f00) {
                return D3DFMT_A4R4G4B4;
            }
        }

        return D3DFMT_UNKNOWN;
    }

    if((pf.flags & k_pf_luminance) != 0) {
        if(pf.rgb_bit_count == 8) {
            return D3DFMT_L8;
        }
        if(pf.rgb_bit_count == 16) {
            return has_alpha ? D3DFMT_A8L8 : D3DFMT_L16;
        }
        return D3DFMT_UNKNOWN;
    }

    if((pf.flags & k_pf_alpha) != 0 && pf.rgb_bit_count == 8) {
        return D3DFMT_A8;
    }

    return D3DFMT_UNKNOWN;
}

std::string_view format_name(D3DFORMAT format) noexcept
{
    switch(format) {
        case D3DFMT_A8R8G8B8:
            return "A8R8G8B8";
        case D3DFMT_X8R8G8B8:
            return "X8R8G8B8";
        case D3DFMT_A8B8G8R8:
            return "A8B8G8R8";
        case D3DFMT_L8:
            return "L8";
        case D3DFMT_A8L8:
            return "A8L8";
        case D3DFMT_L16:
            return "L16";
        case D3DFMT_A8:
            return "A8";
        case D3DFMT_A16B16G16R16:
            return "A16B16G16R16";
        case D3DFMT_A16B16G16R16F:
            return "A16B16G16R16F";
        case D3DFMT_R32F:
            return "R32F";
        case D3DFMT_A32B32G32R32F:
            return "A32B32G32R32F";
        case D3DFMT_DXT1:
            return "DXT1";
        case D3DFMT_DXT3:
            return "DXT3";
        case D3DFMT_DXT5:
            return "DXT5";
        default:
            return "an unrecognised format";
    }
}

int next_level(int size) noexcept
{
    return (std::max)(1, size / 2);
}

// Copies one surface out of the file and into whatever the driver handed back, row by row.
//
// Row by row and not one memcpy, because the destination pitch is the driver's business: it pads
// rows to whatever alignment it likes, and a straight copy of a 96-byte row into a 128-byte stride
// produces a sheared image that still looks vaguely like the picture - which is worse than garbage,
// because it survives a glance.
void copy_rows(std::byte* destination, std::size_t destination_pitch, const std::byte* source, const surface_layout& layout) noexcept
{
    for(std::size_t row = 0; row < layout.rows; ++row) {
        std::memcpy(destination + row * destination_pitch, source + row * layout.row_bytes, layout.row_bytes);
    }
}
} // namespace

namespace tw::skybox::texture
{
std::string_view shape_name(shape form) noexcept
{
    switch(form) {
        case shape::plane:
            return "2D";
        case shape::volume:
            return "volume";
        case shape::cube:
            return "cube";
        default:
            return "nothing";
    }
}

volume_support volumes(IDirect3DDevice9* device) noexcept
{
    volume_support out;

    if(device == nullptr) {
        return out;
    }

    D3DCAPS9 caps {};
    if(FAILED(device->GetDeviceCaps(&caps))) {
        return out;
    }

    out.supported = (caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP) != 0;
    out.max_extent = static_cast<int>(caps.MaxVolumeExtent);

    // Trilinear specifically. A card that offers volumes with point sampling only would draw a baked
    // noise field as a lattice of hard cubes, which is a different artefact from "no volumes at all"
    // and deserves a different message.
    out.filtered = (caps.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) != 0
        && (caps.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) != 0;

    return out;
}

void release(loaded& texture) noexcept
{
    if(texture.object != nullptr) {
        texture.object->Release();
    }

    texture = {};
}

bool create_from_dds(IDirect3DDevice9* device, std::span<const std::byte> bytes, loaded& out, std::string& error) noexcept
{
    out = {};
    error.clear();

    if(device == nullptr) {
        error = "no device";
        return false;
    }

    if(bytes.size() < sizeof(std::uint32_t) + sizeof(dds_header)) {
        error = "too short to be a .dds file";
        return false;
    }

    std::uint32_t magic = 0;
    std::memcpy(&magic, bytes.data(), sizeof(magic));

    if(magic != k_magic) {
        error = "not a .dds file - it does not start with the DDS marker";
        return false;
    }

    dds_header header {};
    std::memcpy(&header, bytes.data() + sizeof(magic), sizeof(header));

    if(header.size != sizeof(dds_header) || header.pixel_format.size != sizeof(dds_pixelformat)) {
        error = "the .dds header is not the size the format defines";
        return false;
    }

    if((header.pixel_format.flags & k_pf_fourcc) != 0 && header.pixel_format.four_cc == four_cc('D', 'X', '1', '0')) {
        error = "this is a DX10-extended .dds - re-export it as a legacy D3D9 format";
        return false;
    }

    const D3DFORMAT format = format_of(header.pixel_format);
    if(format == D3DFMT_UNKNOWN) {
        error = "the pixel format in this .dds is not one D3D9 takes directly";
        return false;
    }

    const int width = static_cast<int>(header.width);
    const int height = static_cast<int>(header.height);

    if(width <= 0 || height <= 0) {
        error = "the .dds declares no size";
        return false;
    }

    const bool is_volume = (header.caps2 & k_caps2_volume) != 0 && header.depth > 1;
    const bool is_cube = (header.caps2 & k_caps2_cubemap) != 0;

    const int depth = is_volume ? static_cast<int>(header.depth) : 1;
    const int levels = (header.flags & k_header_flag_mipmapcount) != 0 ? (std::max)(1, static_cast<int>(header.mip_map_count)) : 1;

    if(is_cube && (header.caps2 & k_caps2_face_mask) != k_caps2_face_mask) {
        error = "this .dds is a cube map with faces missing - all six are needed";
        return false;
    }

    if(is_volume) {
        const volume_support support = volumes(device);
        if(!support.supported) {
            error = "this card cannot sample volume textures at all";
            return false;
        }
        if((std::max)((std::max)(width, height), depth) > support.max_extent) {
            error = "this volume is larger than the card's limit of " + std::to_string(support.max_extent);
            return false;
        }
    }

    const std::byte* cursor = bytes.data() + sizeof(magic) + sizeof(header);
    const std::byte* const limit = bytes.data() + bytes.size();

    // Reads one surface out of the file, advancing the cursor. Null when the file ends early, which
    // is the one failure that cannot be detected from the header: a truncated download has a
    // perfectly good header describing data that is not there.
    const auto take = [&cursor, limit](const surface_layout& layout) -> const std::byte* {
        if(static_cast<std::size_t>(limit - cursor) < layout.bytes()) {
            return nullptr;
        }

        const std::byte* at = cursor;
        cursor += layout.bytes();

        return at;
    };

    out.width = width;
    out.height = height;
    out.depth = depth;
    out.levels = levels;
    out.format = format;

    if(is_volume) {
        IDirect3DVolumeTexture9* volume = nullptr;
        if(FAILED(device->CreateVolumeTexture(static_cast<UINT>(width),
               static_cast<UINT>(height),
               static_cast<UINT>(depth),
               static_cast<UINT>(levels),
               0,
               format,
               D3DPOOL_MANAGED,
               &volume,
               nullptr))) {
            error = "the card refused a " + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth) + " "
                + std::string { format_name(format) } + " volume";
            return false;
        }

        int w = width;
        int h = height;
        int d = depth;

        for(int level = 0; level < levels; ++level) {
            const surface_layout layout = layout_of(format, w, h);

            D3DLOCKED_BOX box {};
            if(FAILED(volume->LockBox(static_cast<UINT>(level), &box, nullptr, 0))) {
                volume->Release();
                error = "could not lock level " + std::to_string(level) + " of the volume";
                return false;
            }

            for(int slice = 0; slice < d; ++slice) {
                const std::byte* source = take(layout);
                if(source == nullptr) {
                    volume->UnlockBox(static_cast<UINT>(level));
                    volume->Release();
                    error = "the file ends before its own header says it should - it is truncated";
                    return false;
                }

                copy_rows(static_cast<std::byte*>(box.pBits) + static_cast<std::size_t>(slice) * static_cast<std::size_t>(box.SlicePitch),
                    static_cast<std::size_t>(box.RowPitch),
                    source,
                    layout);
            }

            volume->UnlockBox(static_cast<UINT>(level));

            w = next_level(w);
            h = next_level(h);
            d = next_level(d);
        }

        out.object = volume;
        out.form = shape::volume;

        return true;
    }

    if(is_cube) {
        IDirect3DCubeTexture9* cube = nullptr;
        if(FAILED(device->CreateCubeTexture(
               static_cast<UINT>(width), static_cast<UINT>(levels), 0, format, D3DPOOL_MANAGED, &cube, nullptr))) {
            error = "the card refused a " + std::to_string(width) + " " + std::string { format_name(format) } + " cube map";
            return false;
        }

        // Face-major: the whole mip chain of +X, then the whole chain of -X, and so on. That is the
        // order the format defines, and it is the opposite of what a level-major loop would produce.
        for(int face = 0; face < 6; ++face) {
            int w = width;
            int h = height;

            for(int level = 0; level < levels; ++level) {
                const surface_layout layout = layout_of(format, w, h);
                const std::byte* source = take(layout);

                if(source == nullptr) {
                    cube->Release();
                    error = "the file ends before its own header says it should - it is truncated";
                    return false;
                }

                D3DLOCKED_RECT rect {};
                if(FAILED(cube->LockRect(static_cast<D3DCUBEMAP_FACES>(face), static_cast<UINT>(level), &rect, nullptr, 0))) {
                    cube->Release();
                    error = "could not lock face " + std::to_string(face) + " of the cube map";
                    return false;
                }

                copy_rows(static_cast<std::byte*>(rect.pBits), static_cast<std::size_t>(rect.Pitch), source, layout);
                cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face), static_cast<UINT>(level));

                w = next_level(w);
                h = next_level(h);
            }
        }

        out.object = cube;
        out.form = shape::cube;

        return true;
    }

    IDirect3DTexture9* plane = nullptr;
    if(FAILED(device->CreateTexture(static_cast<UINT>(width),
           static_cast<UINT>(height),
           static_cast<UINT>(levels),
           0,
           format,
           D3DPOOL_MANAGED,
           &plane,
           nullptr))) {
        error = "the card refused a " + std::to_string(width) + "x" + std::to_string(height) + " " + std::string { format_name(format) }
            + " texture";
        return false;
    }

    int w = width;
    int h = height;

    for(int level = 0; level < levels; ++level) {
        const surface_layout layout = layout_of(format, w, h);
        const std::byte* source = take(layout);

        if(source == nullptr) {
            plane->Release();
            error = "the file ends before its own header says it should - it is truncated";
            return false;
        }

        D3DLOCKED_RECT rect {};
        if(FAILED(plane->LockRect(static_cast<UINT>(level), &rect, nullptr, 0))) {
            plane->Release();
            error = "could not lock level " + std::to_string(level);
            return false;
        }

        copy_rows(static_cast<std::byte*>(rect.pBits), static_cast<std::size_t>(rect.Pitch), source, layout);
        plane->UnlockRect(static_cast<UINT>(level));

        w = next_level(w);
        h = next_level(h);
    }

    out.object = plane;
    out.form = shape::plane;

    return true;
}
} // namespace tw::skybox::texture
