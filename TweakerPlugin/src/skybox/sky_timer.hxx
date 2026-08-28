#pragma once

// How long the sky draw actually takes on the GPU, in microseconds.
//
// Phase 1 ended with the frame cost unmeasured and unmeasurable: the machine it ran on is a Ryzen
// 7 9800X3D with a Radeon RX 9070XT, where a 2008 game showing a frame rate drop would mean a bug
// rather than an expensive shader - and where its absence therefore means nothing at all. Watching
// the frame rate cannot answer the question on hardware like that.
//
// A timestamp around the draw can. It is independent of how fast the rest of the frame is, it does
// not care that the game is old, and it gives a number that can be scaled: a sky costing 0.1 ms
// here is several milliseconds on the integrated graphics somebody will eventually run this on.
//
// Cheap enough to leave on: two timestamps per frame, read back without blocking, and skipped
// entirely on the frames where the previous pair has not landed yet.
namespace tw::skybox::timer
{
// Measuring at all. Off by default, and meant to follow whether the overlay is open: the number is
// only ever read off a UI label, and issuing queries nobody will look at is work for nothing.
void set_enabled(bool value) noexcept;

// Around the draw call, in that order. Safe to call when measuring is off, and when the device does
// not support timestamp queries - the module goes quiet and average_microseconds() stays as it was.
void begin(IDirect3DDevice9* device) noexcept;
void end() noexcept;

// Smoothed over recent frames, or 0 when nothing has been measured yet. Individual frames are noisy
// enough on a modern GPU that a single sample says less than the average does.
[[nodiscard]] float average_microseconds() noexcept;

// Queries live outside any pool and must be released before Reset, like a D3DPOOL_DEFAULT resource.
void on_device_lost() noexcept;
void release_device_resources() noexcept;
} // namespace tw::skybox::timer
