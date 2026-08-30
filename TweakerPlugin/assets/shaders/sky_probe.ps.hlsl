// The diagnostic program: the gradient sky with the axes of sky space drawn on it.
//
// Written for Phase 0, where its job was to prove that a vs_3_0/ps_3_0 pair can be created, bound
// and drawn inside an intercepted draw call, and that the state block hands the device back
// unchanged. All of that passed - see the results section of Docs/Internal/skybox-procedural.md,
// including the answer the markers were there to give: +X right, +Y up, +Z ahead of the player.
//
// Kept as a selectable program rather than deleted. It is the fastest way to tell "the shader path
// is broken" apart from "this particular program looks wrong", and it will be the first thing worth
// running on a machine that is not this one.
//
// Uses c0..c5: g_runtime.y carries the marker intensity, so sky_program lists it as six registers.
#include "sky_common.hlsli"

float4 main(sky_in input) : COLOR0
{
    float3 d = normalize(input.dir);

    float3 col = sky_gradient(d);
    col += light_term(d);

    // +X red, +Y green, +Z blue, plus a ring on the y = 0 great circle. If the ring stands vertical
    // on screen, sky space and the game's world disagree about which axis is up.
    float3 markers = float3(1.0, 0.15, 0.15) * smoothstep(0.995, 0.999, d.x);
    markers += float3(0.15, 1.0, 0.15) * smoothstep(0.995, 0.999, d.y);
    markers += float3(0.25, 0.45, 1.0) * smoothstep(0.995, 0.999, d.z);
    markers += 0.35 * (1.0 - smoothstep(0.0, 0.02, abs(d.y)));

    return float4(col + markers * g_runtime.y, 1.0);
}
