// The constant layout every sky program shares, plus the two pieces of maths all of them start
// from. Included by the .ps.hlsl files; not compiled on its own (the build globs *.hlsl, and this
// is .hlsli precisely so it is not picked up as a program).
//
// Use as much or as little of it as a program needs. The plugin reads the compiled shader's own
// constant table and uploads exactly the registers it declares, so a program that wants nothing but
// g_runtime gets g_runtime and nothing else is touched.
//
// That is not an optimisation. fxc drops a uniform the shader never reads and hands its register to
// one of the shader's own literals, which live in the same register file - so writing a register the
// program does not use would overwrite its constants rather than configure it. See
// skybox/sky_bytecode.
//
// The vertex side is separate and fixed - c0..c3 hold the transposed world-view-projection matrix,
// see sky_cube.vs.hlsl.

// Knobs for the palette, offered to every program that uses it. A line survives only if this
// particular shader actually reads that variable - fxc drops what a program does not use, and the
// plugin resolves these against the compiled shader's own constant table.
//
// The values below are shape, not data: a shared knob starts wherever the program's own palette
// already sits, because this header cannot know whether it is talking to a day sky or a night one.
/* @sky
group = Palette
color = g_zenith      | Zenith           | 0.10,0.28,0.62
color = g_horizon     | Horizon          | 0.78,0.80,0.78
color = g_ground      | Ground           | 0.10,0.11,0.13
color = g_light_color | Light colour     | 1.0,0.95,0.85
param = g_light.x     | Light direction X | 0.349 | -1.0 | 1.0
param = g_light.y     | Light direction Y | 0.419 | -1.0 | 1.0
param = g_light.z     | Light direction Z | 0.838 | -1.0 | 1.0
param = g_light.w     | Light size        | 0.9998 | 0.99 | 1.0
param = g_light_color.w | Light glow      | 320 | 1 | 1000
*/

float3 g_zenith : register(c0);
float3 g_horizon : register(c1);
float3 g_ground : register(c2);
float4 g_light : register(c3);       // xyz = direction in sky space, w = cos(angular radius)
float4 g_light_color : register(c4); // rgb = colour, w = glow exponent
float4 g_runtime : register(c5);     // x = seconds since the shader path started, y = program-specific

struct sky_in {
    float3 dir : TEXCOORD0;
};

// Zenith down to horizon, then a short crossfade into the ground colour below it. The 0.45 exponent
// pulls the horizon band wider than a linear ramp would: on a real sky the interesting gradient is
// all in the first few degrees above the horizon, and a linear one spends most of its range on
// featureless overhead blue.
float3 sky_gradient(float3 d)
{
    float3 col = lerp(g_horizon, g_zenith, pow(saturate(d.y), 0.45));
    return lerp(col, g_ground, saturate(-d.y * 4.0));
}

// Disc plus glow for whatever g_light points at - a sun in a day program, a moon in a night one.
// step() rather than smoothstep for the disc: the edge is one pixel wide at any sane resolution,
// and this is exactly the kind of place where a cube map would have shown its texels instead.
float3 light_term(float3 d)
{
    float cd = dot(d, g_light.xyz);
    return g_light_color.rgb * (pow(saturate(cd), g_light_color.w) * 0.6 + step(g_light.w, cd) * 4.0);
}
