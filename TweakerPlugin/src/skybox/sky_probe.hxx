#pragma once

// What the intercepted frame actually looks like, measured inside the running game.
//
// Phase 1 of Docs/Internal/skybox-geometry.md asks a handful of questions about the frame we insert
// ourselves into, and they have one thing in common: the answers belong to the game, so no amount
// of reading this plugin's own source produces them. Each one changes the design of a second draw
// pass rather than merely informing it:
//
//  - How many times per frame is the sky draw intercepted? The renderer redraws the whole cube on
//    every match, and sky_timer only ever reports one of them (it skips a frame whose previous
//    query pair has not landed). So a count above one means the measured cost is a fraction of the
//    real one, and it multiplies whatever a sprite pass costs on top.
//  - Is the back buffer bound at that moment? If the game intercepts its sky while rendering into
//    something else, sprites drawn "after the blit" land somewhere other than the screen.
//  - Is there a depth buffer? sky_target deliberately unbinds one, but the state outside it is the
//    game's, and it decides whether a mesh could ever occlude itself (Phase 5).
//  - Is Quest3D in software vertex processing? The device is created MIXED (see sky_caps), and
//    eight cube vertices do not care either way while a few thousand sprite vertices would.
//
// Measured, therefore, rather than assumed. The per-frame draw count is free - one increment on a
// path that is about to shade the entire screen anyway - and everything else is a one-shot snapshot
// taken on the first intercepted draw after somebody asks for one, so the hot path stays a counter.
//
// Single-threaded by construction: all three entry points below run on the render thread (the draw
// hooks, EndScene, and the overlay inside it), which is why none of this is behind an atomic the
// way skybox.cxx's texture mirror is.
namespace tw::skybox::probe
{
struct facts {
    bool captured {}; // false until a sky draw has actually been seen

    // Rolled over at the end of each frame. `peak` is what survives a glance at the overlay: the
    // interesting case is rare and the eye would miss it in a live counter.
    int draws_last_frame {};
    int draws_peak {};

    bool on_back_buffer {};
    bool depth_bound {};
    bool software_vertex_processing {};

    int viewport_width {};
    int viewport_height {};
    int target_width {};
    int target_height {};

    // The blend state the game had set when we took the draw. Not needed to restore anything - the
    // D3DSBT_ALL state block does that - but it says what a second, blended pass has to set for
    // itself rather than inherit.
    bool alpha_blend_enabled {};
    unsigned long src_blend {};
    unsigned long dest_blend {};
};

// From the draw interceptor, once it has recognised the sky and before it draws. Hot path.
void observe(IDirect3DDevice9* device) noexcept;

// From the overlay's per-frame tick, which is EndScene. Rolls the counter over - see the comment on
// the definition for why an empty tick is deliberately ignored rather than reported as a zero.
void on_frame() noexcept;

// Throws away the count in flight without rolling it into the statistics. For a device that is
// about to be Reset: the overlay's tick does not run while the device is lost, so without this the
// draws issued across that gap pile into one apparent frame and fabricate a peak.
void discard_frame() noexcept;

// Take a fresh snapshot on the next intercepted sky draw. One is requested implicitly at startup
// and whenever a device binds, so the common case needs no call at all.
void request_snapshot() noexcept;

// Forgets everything, including the peak. For a device change, where the old numbers describe a
// device that no longer exists.
void reset() noexcept;

[[nodiscard]] facts current() noexcept;
} // namespace tw::skybox::probe
