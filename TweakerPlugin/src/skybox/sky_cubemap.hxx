#pragma once

namespace tw::skybox
{
// Where the cube map comes from, and what to do with it on the way in.
struct cubemap_source {
    // Packed-resource key of a horizontal-cross image, e.g. "skyboxes/cloudy_01.png". Used only
    // when `file_path` is empty.
    std::string_view resource_key;

    // Overrides `resource_key` when non-empty. Either a horizontal-cross image, or a directory
    // holding six square face images (see the name table in the .cxx). Relative paths are resolved
    // against the directory the DLL sits in.
    //
    // This is the road to high resolution: a 2048px-per-face cross is an 8192x6144 image, and six
    // of those baked into the DLL would be the better part of a hundred megabytes. On disk they
    // cost nothing.
    std::string_view file_path;

    // Exposure applied when the source is a Radiance .hdr. Ignored for ordinary 8-bit images.
    //
    // stb decodes .hdr, but its own conversion to 8 bits is pow(value*scale, 1/gamma) with a hard
    // clip - so a real sky HDRI, where the sun sits in the thousands while the sky sits near one,
    // comes out as a white blob with a horizon under it. This module tone maps instead: exposure,
    // then a Reinhard rolloff that compresses the sun rather than clipping it, then gamma.
    float hdr_exposure;

    // Faces smaller than this are upscaled with Catmull-Rom on the way to the GPU. 0 leaves the art
    // exactly as authored.
    //
    // Worth being blunt about what this does and does not do: it invents no detail. What it changes
    // is the reconstruction filter. At the magnifications a skybox runs at, the GPU's bilinear
    // magnification is the thing producing the mush - a cubic prescale replaces it with a filter
    // that has negative lobes, so edges keep some acutance instead of ramping linearly between
    // texel centres. Better-looking, not more detailed, and it costs (target/source)^2 memory.
    int min_face_size;
};

// The file stems accepted for the six faces of a directory-based cube map, in D3DCUBEMAP_FACES
// order (+X, -X, +Y, -Y, +Z, -Z), three spellings each. Exposed so the catalog can tell a folder of
// faces from any other folder without keeping a second copy of the list that could drift from this
// one.
[[nodiscard]] std::span<const std::array<std::string_view, 3>> face_stems() noexcept;

struct cubemap_result {
    // Null on failure. The caller owns the reference.
    IDirect3DCubeTexture9* texture = nullptr;

    // What the source could have supplied, and what the device actually agreed to allocate. They
    // differ when CreateCubeTexture refused on memory and the halving retry stepped down - which is
    // worth surfacing rather than hiding, because the result is a visibly softer sky and the user
    // has no other way to know why.
    int requested_face_size = 0;
    int face_size = 0;

    [[nodiscard]] bool downscaled() const noexcept
    {
        return texture != nullptr && face_size > 0 && requested_face_size > face_size;
    }
};

// Builds a cube texture from `source`.
//
// A cross image must be exactly four tiles wide and three tall with square tiles:
//
//         +Y
//     -X  +Z  +X  -Z
//         -Y
//
// The four corner tiles are unused and never read. A face directory must hold six square images of
// equal size. Anything else is rejected rather than guessed at.
//
// The result is D3DPOOL_MANAGED with a single mip level: the sky fills the screen, so it is
// magnified far more often than minified, and a single level keeps both the upload and the
// device-lost story trivial (managed resources survive Reset on their own).
//
// Returns a result whose `texture` is null - and logs why - on a missing source, an undecodable
// image, a bad layout, a face size beyond what the device supports, or a CreateCubeTexture that
// failed even at the smallest fallback size.
[[nodiscard]] cubemap_result create_cubemap(IDirect3DDevice9* device, const cubemap_source& source) noexcept;
} // namespace tw::skybox
