#pragma once

// Rendering the sky at a fraction of the viewport and stretching it back.
//
// The measurement that motivated this: the DyingLight port costs 5-7 ms per frame on an RX 9070 XT,
// and every one of those pixels is a full-screen pixel. The sky draw is intercepted where the game
// draws its own sky sphere - before the track, with depth testing off - so there is nothing for
// early-Z to reject and the expensive shader runs for the whole screen, most of which the track
// then covers.
//
// Half resolution is four times fewer pixels, and on a sky - gradients, fog, glow, nothing with a
// hard edge - the difference is genuinely hard to see. But "hard to see" is not "invisible", and
// somebody with a fast card and a cheap program should not be paying for a compromise they do not
// need. Hence a setting rather than a decision: this module does nothing at all at 100%.
//
// Only the shader path uses it. A cube map is one texture lookup per pixel and would gain nothing
// from being sampled twice.
namespace tw::skybox::target
{
// Redirects rendering into a scaled-down target. Returns false when the caller should simply draw
// where it already is - at 100%, or when the target could not be created, which is a reason to
// render normally rather than to skip the sky.
//
// On success the viewport is the target's, so the caller draws exactly as it otherwise would.
[[nodiscard]] bool begin(IDirect3DDevice9* device, int scale_percent) noexcept;

// Puts the previous render target, depth buffer and viewport back, then stretches what was drawn
// over the viewport it came from. Only call after begin() returned true.
void end(IDirect3DDevice9* device) noexcept;

// D3DPOOL_DEFAULT, so this has to go before a Reset and come back after.
void on_device_lost() noexcept;
void release_device_resources() noexcept;
} // namespace tw::skybox::target
