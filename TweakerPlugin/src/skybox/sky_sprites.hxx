#pragma once

#include "skybox/sky_bytecode.hxx"

// The sky's geometry layer: flat quads on a shell around the camera, drawn as a second pass after
// the sky itself. Docs/Internal/skybox-geometry.md has the reasoning; the short version is that
// every other layer of the sky is a function of direction and therefore expressible in one pixel
// shader, while a cloud is an object and is not.
//
// Where the pass runs is settled and worth not re-deriving: inside sky_renderer's state capture (so
// the game gets its state back untouched) and *after* sky_target::end() (so it draws at full
// resolution onto the restored back buffer, over the already-upscaled sky). Both paths call it
// through renderer::attach_extra_pass.
//
// What this module does *not* decide any more is what the layer looks like. Its sprite count, its
// size and every constant its shader reads come from a `sprites` layer of a `.sky` package - as
// parameters the author declared and bindings the author wrote. There is no second set of knobs
// here and no manual light: a cloud is lit by the sky's own lights, through the same shared block
// every other layer reads, and that is what makes it look like it belongs to the sky behind it
// rather than pasted over it. See sky_shared.
namespace tw::skybox
{
struct sky_program;
}

namespace tw::skybox::sprites
{
// The shader pair built into the plugin, as packed resource keys. A `sprites` layer that names no
// shader of its own draws with these; sky_program reflects the pixel half exactly as it does a
// compiled sky, so a manifest can name its variables and bind to them either way.
//
// A fallback, not the normal case. A sky that ships its own `shaders/clouds.vs.hlsl` and
// `.ps.hlsl` is the readable one - the author can see where the clouds come from and change it -
// and these exist so that a package which has not bothered still draws something.
inline constexpr std::string_view k_builtin_vertex_key = "shaders/sky_sprite.vs.fxo";
inline constexpr std::string_view k_builtin_pixel_key = "shaders/sky_sprite.ps.fxo";

// Registers the pass with sky_renderer. Call once at start-up, before any device exists.
void initialize() noexcept;

// Releases device resources; from the unbind listener, while that device is still alive. The
// vertex/index buffers are D3DPOOL_MANAGED and ride out a Reset, so there is no on_device_lost().
void release_device_resources() noexcept;

// Builds or rebuilds whatever the current layout needs - the atlas, the vertex and index buffers -
// when a knob has moved since the last one. Cheap and a no-op when nothing changed.
//
// Called once per frame from the frame tick, and deliberately **not** from the draw pass. The draw
// pass runs inside sky_renderer's state capture, inside the interception of one of the game's own
// draw calls; building there means allocating buffers and baking a texture in the middle of somebody
// else's frame. It also puts a hard ceiling on what generation may ever cost, which is the wrong
// ceiling to accept now that placement is heading for a script (Docs/Internal/skybox-geometry.md).
//
// Render thread, outside any interception. The draw pass only consumes what this leaves behind, and
// draws the previous buffer on the frame a rebuild is still pending.
void prepare(IDirect3DDevice9* device) noexcept;

// The `sprites` layer this pass draws, or null to draw nothing.
//
// One call rather than a setter per knob, because the program already carries everything: its
// shaders (its own compiled pair, or the built-ins above), the constants its parameters and bindings
// produced, and the handful of parameters that are properties of this layer *kind* rather than of
// its shader. Those are read here, since what a `sprites` layer means is this module's to know.
//
// Properties understood:
//
//   count       how many sprites
//   size        angular half-size of one, in degrees
//   clumps      how many groups they are gathered into
//   spread      how far around its clump's centre a sprite may fall, in degrees
//   scatter     0 leaves the clumps on an even lattice, 1 lets each wander a full cell off it
//   elongation  largest aspect ratio a clump may take; 1 is round
//   floor       lowest elevation a sprite may occupy, in degrees
//   ceiling     highest
//
// All of them are properties rather than shader constants because they decide what is *in* the
// vertex buffer, not how it is shaded - moving one rebuilds a few tens of kilobytes, which is fine
// for a slider and is why the rebuild is deferred to the next draw rather than done here.
//
// Call again whenever a knob moves. It is a walk over a dozen parameters, on the overlay tick.
void set_layer(const sky_program* program) noexcept;

// Whether the pass draws at all. False unless the sky currently selected is a package with an
// enabled `sprites` layer - a cube map has no clouds, and neither does a lone .hlsl, because
// neither has anywhere to say what the clouds would be lit by.
[[nodiscard]] bool enabled() noexcept;

// How many sprites the vertex buffer currently holds, and whether the pass is actually drawing.
// For the overlay: "enabled" and "working" are different claims, and a shader the device refused
// to create should not look like a count of zero.
[[nodiscard]] int live_count() noexcept;
[[nodiscard]] bool ready() noexcept;
} // namespace tw::skybox::sprites
