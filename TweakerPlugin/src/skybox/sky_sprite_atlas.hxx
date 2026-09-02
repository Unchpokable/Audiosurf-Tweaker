#pragma once

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
inline constexpr int k_tiles_x = 2;
inline constexpr int k_tiles_y = 2;
inline constexpr int k_tile_count = k_tiles_x * k_tiles_y;
inline constexpr int k_tile_size = 256;

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

[[nodiscard]] tile_rect tile(int index) noexcept;

// Releases the texture. From the unbind listener, while that device is still alive.
void release_device_resources() noexcept;
} // namespace tw::skybox::sprite_atlas
