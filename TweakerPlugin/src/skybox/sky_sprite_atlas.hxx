#pragma once

#include "skybox/sky_image.hxx"

// The sprite layer's texture atlas, baked on the CPU at load - Docs/Internal/skybox-geometry.md,
// phase 3.
//
// A grid of cloud tiles in one texture. Each tile carries a *density* in alpha and a *surface
// normal* in rgb, both derived from the same fbm height field: alpha gives the sprite a silhouette
// with lobes and gaps instead of a circle, and the normal makes light break over that shape instead
// of gliding across a smooth ball.
//
// Baked rather than shipped, and baked rather than evaluated per pixel.
//
// Shipping it would mean a megabyte of PNG in the DLL for something an fbm produces in a few
// milliseconds, and it would freeze the look at whatever the art was. Evaluating per pixel is what
// phase 2 did, and is what a sky program already does for every other layer - but a sprite is small
// on screen and there are hundreds of them, so the same noise would be recomputed for the same
// texel hundreds of times a second. A tile is computed once and then read.
//
// This is the "bake it" idea that Docs/Internal/skybox-procedural.md rejected for the sky as a
// whole, and it is worth being clear about why it is right here and wrong there. Baking a whole sky
// costs hundreds of megabytes and loses every animation. A tile is 256 pixels square, and nothing
// is frozen by it: the sprite moves, turns and is lit at run time - only the lumps on its surface
// are fixed, and those would not have moved anyway.
namespace tw::skybox::sprite_atlas
{
// How many distinct clouds the atlas holds, and the grid it lays them out in. Four is enough that
// the eye stops matching them up once each is also randomly rotated and scaled, and small enough
// that the whole thing is one megabyte with a full mip chain.
// What the plugin's own bake produces. A package is free to ask for a different count and size
// through its `fill` block, which is why nothing below reads these except the built-in path.
inline constexpr int k_builtin_tiles = 4;
inline constexpr int k_builtin_tile_size = 256;

// How many distinct sprite images the atlas currently holds, and how big each is. Runtime rather
// than compile-time because a package declares them: `"fill": { "tiles": 12, "size": 128 }`.
//
// Untextured (see set_none), both answer for the one notional tile the layer still has: a count of
// 1, so a generator choosing a tile index chooses the only one there is, and a size of 0, because
// there is no texel for it to be the size of.
[[nodiscard]] int tile_count() noexcept;
[[nodiscard]] int tile_size() noexcept;

// The grid `count` tiles are laid out in: `columns` across, filled left to right and then top to
// bottom. Squarest rather than a single row, because a texture twelve tiles wide and one tall wastes
// most of a mip chain on a dimension that is already 1.
//
// Public because a package's `texture` fill is cut on this same grid. The sheet an author draws and
// the atlas the sprites sample have to agree about which cell is tile 3, and they agree by
// construction only while there is one rule - two copies of ceil(sqrt(n)) would agree until one of
// them was improved.
struct grid_shape {
    int columns;
    int rows;
};

[[nodiscard]] grid_shape grid_for(int count) noexcept;

// Whether this layer has no atlas at all. Asked *before* ensure(), because null from ensure()
// otherwise means the device refused a texture, and the two want opposite responses: one is a layer
// that paints itself, the other is a layer that cannot draw.
[[nodiscard]] bool untextured() noexcept;

// Replaces the atlas with tiles a package's fill() produced.
//
// Takes float layers and quantises here, at the one point where the chain has to become 8-bit - see
// sky_image on why the composite itself is float throughout.
//
// The next ensure() rebuilds the texture from these. Passing an empty span goes back to the built-in
// bake, which is what selecting a sky without a fill block has to do.
void set_tiles(std::span<const image::layer> tiles) noexcept;

// Drops the atlas entirely: `"fill": { "kind": "shader" }`, where the sprite's own pixel shader
// paints the quad and there is nothing to sample.
//
// A third state rather than an empty atlas, because the two differ everywhere it matters. An empty
// atlas would still be a texture the device has to give us, still a failure when it will not, and
// still a grid of cells to inset within - none of which means anything to a layer that never
// samples one.
void set_none() noexcept;

// Creates the texture against `device` if it does not exist, and returns it. Null on failure, which
// the caller should treat as "draw nothing" rather than as a reason to retry every frame.
//
// D3DPOOL_MANAGED, so it survives a Reset; the only thing that invalidates it is a different device.
[[nodiscard]] IDirect3DTexture9* ensure(IDirect3DDevice9* device) noexcept;

// Where tile `index` sits in the atlas, in texture coordinates. The inset is deliberate - see the
// implementation: a tile's outermost texels are shared with its neighbour by bilinear filtering,
// and sampling strictly inside them is what keeps one cloud out of another.
struct tile_rect {
    float u0;
    float v0;
    float u1;
    float v1;
};

// Untextured, every index answers with the whole unit square and no inset: there is no neighbour to
// bleed in from, and a shader painting its own sprite wants coordinates running corner to corner.
[[nodiscard]] tile_rect tile(int index) noexcept;

// Releases the texture. From the unbind listener, while that device is still alive.
void release_device_resources() noexcept;
} // namespace tw::skybox::sprite_atlas
