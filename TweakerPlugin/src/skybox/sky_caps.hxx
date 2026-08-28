#pragma once

// One-shot report of what Audiosurf's D3D9 device is actually able to do.
//
// Phase 0 of the procedural sky work (Docs/Internal/skybox-procedural.md) rests on a handful of
// facts that were established by reversing the game and by compiling shaders offline, but never
// observed on a running machine: which shader models the device reports, whether it will hand out a
// render-target cube map, and whether the behaviour flags at runtime match the ones read out of
// Aco_DX8_DirectGraphicsChannel::CreateD3D. This prints all of it, once, so the plan can stop
// resting on inference.
//
// Log-only, and therefore debug-build-only: TW_LOG_* compiles to nothing in Release, which is what
// the pre-build hook produces. See the testing note in skybox-replacer.md.
namespace tw::skybox::caps
{
// Cold path, once per device bind. Safe to call with a null device (does nothing).
void report(IDirect3DDevice9* device) noexcept;

// Lets the next bind report again - a device recreation is exactly when these answers could change
// (a different adapter, a switch to a software device).
void reset() noexcept;
} // namespace tw::skybox::caps
