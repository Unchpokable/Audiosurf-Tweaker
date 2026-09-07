#pragma once

#include "skybox/sky_bytecode.hxx"
#include "skybox/sky_shader.hxx"

namespace tw::skybox::renderer
{
// One stretch of the pixel constant file whose contents this frame decides. Uploaded last, over
// whatever the program's own stored copy of those registers held - correct either way, and one call
// rather than a copy of the whole file.
struct frame_block {
    int first {};                  // first register
    std::span<const float> values; // four floats per register
};

// Draws a unit cube around the camera, textured with `cube`, in place of whatever draw call the
// caller intercepted. `orientation` rotates the sky in world space (see skybox_config's yaw/pitch
// and z_up); pass an identity matrix for "as authored".
//
// Everything the game had set is restored before this returns - but only what was actually changed,
// and only where it actually differed. Every write goes through a framework::d3d9::state_scope,
// which reads each state before setting it and puts it back on the way out; the caller's next draw
// sees exactly the state it would have seen had the sky draw run normally. See d3d9_state.hxx for
// why that replaced the D3DSBT_ALL state block this used to capture and apply per frame.
//
// Device resources (vertex and index buffer) are created lazily on the first call and rebuilt
// whenever `device` differs from the one they were made against. Returns false - having
// changed nothing - if that creation fails or the required transforms are unreadable, which the
// caller should treat as "let the game draw its own sky this frame".
[[nodiscard]] bool draw(IDirect3DDevice9* device, IDirect3DCubeTexture9* cube, const D3DMATRIX& orientation) noexcept;

// The same cube as draw(), painted by a vs_3_0/ps_3_0 pair instead of a cube map - the procedural
// sky path (Docs/Internal/skybox-procedural.md). No image is involved at any point, which is what
// makes it sharp at any resolution and free of memory.
//
// `pixel_constants` holds four floats per register starting at c0, and `runs` says which stretches
// of it the program actually declared. Only those are uploaded, and that is not an optimisation: a
// register the program does not read belongs to the shader's own literals, so writing one would
// corrupt the program rather than configure it. See sky_program::constant_runs and sky_bytecode.
//
// `frame_constants` is whatever this frame decides rather than the palette - the clock, the music -
// each block naming its own first register. Passed separately rather than patched into
// `pixel_constants` so the caller can hand over the program's own array untouched instead of copying
// the whole register file every frame, and carrying its own register index rather than assuming one,
// because where these live belongs to the shader ABI, which this file has no business knowing.
//
// A list rather than one block, and that is not generality for its own sake: the clock and the music
// are two blocks of the same kind, and giving each its own parameter pair would have meant a third
// pair for the third one. Blocks the program did not declare are simply left out by the caller.
//
// Same contract as draw() otherwise: everything it sets it puts back, false means nothing was
// changed and the game should draw its own sky.
// `scale_percent` renders the sky at a fraction of the viewport and stretches it back (see
// sky_target); 100 draws straight to the back buffer with nothing in between.
//
// `samplers` is what the layer's shader reads from texture stages - a baked noise volume, a gradient
// ramp, a star chart. Empty for every sky that samples nothing, which is most of them. Bound through
// the scope like everything else here, so the game's own stage state comes back untouched.
[[nodiscard]] bool draw_program(IDirect3DDevice9* device,
    IDirect3DVertexShader9* vertex_shader,
    IDirect3DPixelShader9* pixel_shader,
    std::span<const float> pixel_constants,
    std::span<const bytecode::register_run> runs,
    std::span<const frame_block> frame_constants,
    std::span<const shader::sampler> samplers,
    int scale_percent,
    const D3DMATRIX& orientation) noexcept;

// An extra pass drawn on top of the sky, inside the sky's own state scope and at full resolution.
//
// Both of those are the whole point of routing it through here rather than letting a caller draw
// after draw()/draw_program() returns. Inside the scope means the pass may set whatever state it
// likes - through a scope of its own, which restores to the sky's and lets the sky's restore to the
// game's - and the game still gets its own back untouched; after draw_program's blit means it lands on
// the restored back buffer at native resolution, over an already-upscaled sky - so a sprite keeps
// the sharp edge that the reduced-resolution sky deliberately gives up. At 100% there is no blit
// and the same call site simply means "after the cube".
//
// `world_view_projection` is the matrix the sky itself was drawn with, so the pass shares the sky's
// object space: a point at unit distance from the origin sits on the sky's own shell.
//
// One pass, set once at start-up. This is a place to hang the geometry layer, not a general plugin
// point - see Docs/Internal/skybox-geometry.md.
using extra_pass_fn = void (*)(IDirect3DDevice9* device, const D3DMATRIX& world_view_projection);
void attach_extra_pass(extra_pass_fn fn) noexcept;

// Forwards to the timer and the render target, which hold the D3DPOOL_DEFAULT resources that do not
// survive IDirect3DDevice9::Reset. Wired to the pre-Reset listener; this module's own buffers are
// D3DPOOL_MANAGED and deliberately kept, and since the state block went away it holds nothing else.
void on_device_lost() noexcept;

// Releases everything against the currently held device. Must run while that device is still
// alive - i.e. from the unbind listener, not from a later frame.
void release_device_resources() noexcept;
} // namespace tw::skybox::renderer
