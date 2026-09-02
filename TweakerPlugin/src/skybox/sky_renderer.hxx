#pragma once

#include "skybox/sky_bytecode.hxx"

namespace tw::skybox::renderer
{
// Draws a unit cube around the camera, textured with `cube`, in place of whatever draw call the
// caller intercepted. `orientation` rotates the sky in world space (see skybox_config's yaw/pitch
// and z_up); pass an identity matrix for "as authored".
//
// Everything the game had set is restored before this returns: the whole device state is captured
// into a D3DSBT_ALL state block on the way in and applied on the way out, so the caller's next draw
// sees exactly the state it would have seen had the sky draw run normally.
//
// Device resources (vertex/index buffer, state block) are created lazily on the first call and
// rebuilt whenever `device` differs from the one they were made against. Returns false - having
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
// `runtime` is the four floats of the per-frame register and `runtime_register` says where they go,
// or the span is empty when the program does not declare it. Passed separately rather than patched
// into `pixel_constants` so the caller can hand over the program's own array untouched instead of
// copying the whole register file every frame - and passed as an index rather than assumed, because
// which register that is belongs to the shader ABI, which this file has no business knowing.
//
// Same contract as draw() otherwise: full state capture and restore, false means nothing was
// changed and the game should draw its own sky.
// `scale_percent` renders the sky at a fraction of the viewport and stretches it back (see
// sky_target); 100 draws straight to the back buffer with nothing in between.
[[nodiscard]] bool draw_program(IDirect3DDevice9* device,
    IDirect3DVertexShader9* vertex_shader,
    IDirect3DPixelShader9* pixel_shader,
    std::span<const float> pixel_constants,
    std::span<const bytecode::register_run> runs,
    std::span<const float> runtime,
    int runtime_register,
    int scale_percent,
    const D3DMATRIX& orientation) noexcept;

// An extra pass drawn on top of the sky, inside the same state capture and at full resolution.
//
// Both of those are the whole point of routing it through here rather than letting a caller draw
// after draw()/draw_program() returns. Inside the capture means the pass may set whatever state it
// likes and the game still gets its own back untouched; after draw_program's blit means it lands on
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

// Drops the state block, which is the only piece that does not survive IDirect3DDevice9::Reset.
// Wired to the pre-Reset listener; the buffers are D3DPOOL_MANAGED and deliberately kept.
void on_device_lost() noexcept;

// Releases everything against the currently held device. Must run while that device is still
// alive - i.e. from the unbind listener, not from a later frame.
void release_device_resources() noexcept;
} // namespace tw::skybox::renderer
