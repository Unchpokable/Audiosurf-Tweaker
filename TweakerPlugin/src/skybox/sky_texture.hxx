#pragma once

// Turning the bytes of a `.dds` file into a D3D9 texture object.
//
// # Why this exists rather than D3DXCreateTextureFromFile
//
// Two reasons, and the second is the one that settles it.
//
// D3DX is a dead dependency: the game ships four `d3dx9_3x.dll` and links none of them, and a modern
// Windows has none at all. That alone would only be an argument for shipping one.
//
// The one that settles it: **every D3DX loader takes a path.** A `.sky` package may be a zip, and
// nothing inside one has a path - see sky_vfs. A loader that reads bytes works for both forms of the
// format; a loader that opens files works for one of them and looks like it works for both until
// somebody zips their sky.
//
// What is left after that argument is a header parse and a memcpy, which is what this is. DDS is
// four bytes of magic, 124 bytes of header, and the surfaces one after another.
//
// # What it does not do
//
// No conversion. A format this build cannot hand straight to D3D9 is refused with a message naming
// it, rather than silently becoming something else - an author who exported the wrong thing needs to
// know that, and a converter here would be a second, worse copy of a texture tool they already have.
namespace tw::skybox::texture
{
// What the file turned out to hold. Read from the DDS header rather than declared in the manifest,
// deliberately: the file already says which it is, and a manifest that said so too would be a second
// source of truth able to disagree with the first.
enum class shape {
    none,
    plane,  // an ordinary 2D texture
    volume, // a 3D texture - what a noise field is baked into
    cube,   // six faces
};

struct loaded {
    // The base pointer is what SetTexture takes, whichever shape it is.
    IDirect3DBaseTexture9* object {};

    shape form { shape::none };

    int width {};
    int height {};
    int depth {}; // 1 for anything but a volume
    int levels {};

    D3DFORMAT format { D3DFMT_UNKNOWN };

    [[nodiscard]] bool valid() const noexcept
    {
        return object != nullptr;
    }
};

// Creates a texture from the contents of a .dds file.
//
// D3DPOOL_MANAGED, so it survives a device Reset - the same bargain the sprite atlas strikes, and for
// the same reason: nothing here is a render target and the driver's copy costs only address space.
//
// False with `error` set on anything that is not a DDS this build can hand to D3D9. The message is
// meant to be shown to the package's author, so it names what was wrong with *their* file.
[[nodiscard]] bool create_from_dds(
    IDirect3DDevice9* device, std::span<const std::byte> bytes, loaded& out, std::string& error) noexcept;

void release(loaded& texture) noexcept;

// Whether this device can sample a volume at all, and how large. Asked once and cached: a card that
// will not take a 3D texture is not going to start, and a sky that needs one has to say so rather
// than draw black.
struct volume_support {
    bool supported {};
    bool filtered {}; // trilinear between voxels; without it a baked noise field is a lattice of cubes
    int max_extent {};
};

[[nodiscard]] volume_support volumes(IDirect3DDevice9* device) noexcept;

[[nodiscard]] std::string_view shape_name(shape form) noexcept;
} // namespace tw::skybox::texture
