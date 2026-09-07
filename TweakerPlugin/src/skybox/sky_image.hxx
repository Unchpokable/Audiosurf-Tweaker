#pragma once

// Float RGBA image layers, and the operations a package's `fill()` script composes them with.
//
// Separate from the Lua binding on purpose: everything here is ordinary image maths with no VM, no
// device and no package around it, so it can be run and checked offline. That matters more than
// usual here - every one of the four structural bugs in the original atlas bake lived in exactly
// this kind of code, and every one was found by an offline harness rather than by looking at it.
//
// # Why float and not 8-bit
//
// A composite is a dozen operations deep by the time an author is done, and 8 bits accumulates
// banding across them. The atlas carries a **surface normal** in RGB, where banding does not read as
// posterised colour - it reads as faceting on the lighting, which is the exact artefact the normals
// were reworked to remove. Quantisation happens once, on the way into the texture.
//
// # What is here and what is not
//
// The operations are deliberately few and orthogonal: make, load, resize, composite, and the four
// primitives the cloud bake needs. Handing an author forty functions would be forty promises to keep
// working; these compose into the same results.
//
// The noise primitives in particular carry lessons rather than just code. `fbm` rotates each octave
// (a lattice noise summed on one axis makes visible diagonal creases); `normalize` thresholds by a
// *quantile* of the tile's own distribution (an absolute threshold against a field with a floating
// mean gave four tiles at 8.6 / 19.9 / 27.7 / 8.8 percent coverage); `normals` differences the
// *unclamped* field across a wide stencil (a saturated field has no gradient, so a clamped one
// yields a flat cutout with a glowing rim, and a one-texel stencil is dominated by the finest octave
// because every octave of an fbm contributes equally to its derivative). An author starting from
// scratch would rediscover all four.
namespace tw::skybox::image
{
// Rows top to bottom, four floats per texel, straight (non-premultiplied) alpha.
//
// Straight rather than premultiplied because that is what the cloud shader expects, and the reason
// is physical: everything a cloud sends towards the eye is scattered by its own material, so it
// scales with how much material there is. Premultiplying was tried and was a mistake.
struct layer {
    int width {};
    int height {};
    std::vector<float> pixels; // width * height * 4

    [[nodiscard]] bool valid() const noexcept
    {
        return width > 0 && height > 0 && pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }

    [[nodiscard]] float* at(int x, int y) noexcept
    {
        return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
    }

    [[nodiscard]] const float* at(int x, int y) const noexcept
    {
        return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
    }

    // Clamped rather than wrapped. A tile is a sprite, not a repeating pattern: sampling off its edge
    // should reach the nearest texel of the same cloud, not the opposite side of it.
    [[nodiscard]] const float* clamped(int x, int y) const noexcept
    {
        return at(std::clamp(x, 0, width - 1), std::clamp(y, 0, height - 1));
    }
};

[[nodiscard]] layer make(int width, int height, std::array<float, 4> fill = {});

// Decodes an image from bytes. PNG, JPEG, TGA, BMP and the rest of what stb reads, plus HDR - which
// arrives already linear and is the one case where the float buffer is carrying something 8 bits
// could not.
//
// From memory rather than from a path, because a package may be an archive: the bytes come from
// package::file_system and this never learns where they were.
[[nodiscard]] bool decode(std::span<const std::byte> bytes, layer& out);

// Bilinear. For an atlas tile the source is usually larger than the target, so this is a downscale
// and box-filtering would be better - but a tile is square and small and the difference does not
// survive the threshold that follows.
[[nodiscard]] layer resize(const layer& source, int width, int height);

// Cuts a sheet into `count` cells on a `columns` x `rows` grid - left to right, then top to bottom -
// and scales each to `size` square.
//
// What `"fill": { "kind": "texture" }` does with the image a package ships. The grid is passed in
// rather than derived here, because the one place entitled to decide it is the atlas the cells are
// going into: sprite_atlas::grid_for.
//
// Whole texels, and any remainder past the last full cell is ignored - a sheet 1023 wide cut into
// three keeps 341 of each column and drops the last pixel, rather than sliding every cell after the
// first half a texel off its neighbour.
//
// Empty when the grid does not fit: a sheet with fewer texels than cells cannot be cut into them,
// and returning degenerate tiles would turn an author's arithmetic mistake into a rendering one.
[[nodiscard]] std::vector<layer> slice(const layer& sheet, int columns, int rows, int count, int size);

enum class blend {
    normal,   // src over dst, by src alpha
    add,      // dst + src
    multiply, // dst * src
    screen,   // 1 - (1-dst)(1-src)
    max_of,   // per channel
    replace,  // src, alpha included - for building a channel rather than compositing onto one
};

struct composite_options {
    // Where the source lands in the destination, in destination texels. The source is sampled with
    // bilinear filtering, so these need not be whole numbers.
    float x {};
    float y {};

    float scale { 1.f };
    float rotation {}; // radians, about the placed centre

    float opacity { 1.f };

    blend mode { blend::normal };

    // Which channels the operation is allowed to write. Building an alpha out of one image and an rgb
    // out of another is the ordinary case in a composite, and without this it would need a temporary
    // and two more operations.
    std::array<bool, 4> channels { true, true, true, true };
};

void composite(layer& dst, const layer& src, const composite_options& options);

struct fbm_options {
    int octaves { 5 };
    float frequency { 2.8f };
    float warp { 0.6f };       // domain warp strength; 0 leaves the field unwarped
    float warp_frequency { 1.8f };
    unsigned int seed {};

    // Which channel receives it. Noise is a scalar field, and writing it to all four at once is
    // almost never what a composite wants.
    int channel { 3 };
};

// Fills one channel with fractional Brownian motion over the layer's own extent.
//
// Each octave's domain is rotated by ~40 degrees - not a fraction of a right angle, so repeated
// application never returns to axis alignment. Without it a lattice noise summed on one axis leaves
// straight diagonal creases, which is what "billow" made unmistakable and what plain summation makes
// merely visible.
void fbm(layer& target, const fbm_options& options);

struct radial_options {
    float inner { 0.60f };   // fraction of the half-extent where the falloff starts
    float feather { 0.36f }; // and where it reaches full
    float depth { 0.50f };   // how much is taken away at the edge
    int channel { 3 };
};

// Subtracts a radial falloff, so a tile fades out before its own boundary.
//
// **Subtracts**, and that is the whole of it. Multiplying by a falloff continuously raises the
// effective threshold towards the edge instead of lowering the surface under a fixed one - the first
// version did that and produced tiles that were 10 percent covered and 0.1 percent solid: wisps with
// no body anywhere.
void radial(layer& target, const radial_options& options);

struct normalize_options {
    // The fraction of the tile that should end up above zero. Asked for as a quantile of the field's
    // own distribution rather than as an absolute cut, because the field's mean floats: four tiles
    // thresholded absolutely came out at 8.6, 19.9, 27.7 and 8.8 percent coverage.
    float coverage { 0.42f };

    // Contrast the transition works over, as a fraction of the distance from the cut to the field's
    // 97th percentile. This is what makes the slope in `normals` a number with a meaning.
    float band { 0.55f };

    int channel { 3 };
};

// Rescales a channel so that `coverage` of it is above zero and one unit is the silhouette
// transition. Deliberately **not** clamped: what comes out is the surface, and clamping it here is
// what would flatten the normals computed from it.
void normalize(layer& target, const normalize_options& options);

struct normals_options {
    // Texels between the samples of the central difference. One is dominated by the finest octave -
    // every octave of an fbm contributes equally to the derivative, amplitude halving as frequency
    // doubles - and lighting that gives foil rather than cloud. Widening it averages the fine octaves
    // away and leaves the lobes, which are what is worth lighting.
    int span { 3 };

    float relief { 1.4f };

    int height_channel { 3 };
};

// Writes a surface normal into rgb, from a height held in one channel.
//
// The height must be the *unclamped* field - see normalize. A saturated one has no gradient, so its
// normals are flat throughout the interior and violent along the outline, and a sprite lit by them is
// a flat cutout with a glowing rim.
void normals(layer& target, const layer& height, const normals_options& options);

struct alpha_options {
    int height_channel { 3 };
    float softness { 1.f }; // >1 widens the transition, <1 sharpens it
};

// Turns the unclamped surface into an opacity: clamp, then smoothstep.
void alpha_from_height(layer& target, const layer& height, const alpha_options& options);

// 8-bit BGRA, the form a D3D9 A8R8G8B8 texture wants. The single quantisation in the chain.
void to_bgra(const layer& source, std::vector<std::uint32_t>& out);
} // namespace tw::skybox::image
