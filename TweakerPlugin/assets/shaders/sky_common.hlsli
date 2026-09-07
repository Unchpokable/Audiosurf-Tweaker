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

// What the playing song is doing, updated every frame. Declare only what you read: a program that
// mentions none of these gets none of them uploaded and pays nothing, and the plugin does not even
// read the game's channels for a sky that does not ask.
//
// These are engine-written, not parameters. Do not put an `@sky` annotation on them - an annotation
// would make the overlay write the register too, and whichever wrote last would win.
//
// c208 and up because the low registers are handed out by hand and collide; c208..c223 is reserved
// for blocks like this one.
//
// **Always multiply by g_music.w.** It is 0 whenever the values are not real - in a menu, between
// songs, before the graph is reachable - and a sky that ignores it freezes on whatever it last saw
// instead of settling.
// x is the game's own total and is **weighted towards the top end**: it sums twelve linear bands and
// nine of them sit above 5.4 kHz, so a cymbal out-totals a bass line. Drive things from y.
float4 g_music : register(c208);      // x = raw total, y = body (0..3.6 kHz, smoothed - USE THIS),
                                      // z = onset pulse, w = valid
float4 g_music_time : register(c209); // x = seconds into the song, y = its length, z = 0..1 through it,
                                      // w = seconds since the last onset
// The spectrum, in the only four groups the game's FFT can actually distinguish. Its twelve bands are
// linear - 1808.7 Hz each, measured - so band 0 alone holds every fundamental in the music and nine
// of the twelve sit above 5.4 kHz. Do not expect a kick drum here: it shares `x` with the vocals.
//
// Smoothed, like everything else here: a pixel shader has no state and cannot filter a value itself,
// so a raw per-frame number would be noise with no way to remove it.
float4 g_music_eq : register(c210);   // x = 0..1.8 kHz, y = 1.8..3.6, z = 3.6..5.4, w = 5.4 kHz and up

// The same body held over four increasing windows: half a second, a second and a half, four seconds,
// ten. One-pole time constants, so read them as "the last second or so", not as a boxcar.
//
// What g_music.y cannot do. On dense, many-voiced music the fast body changes every note, and an
// effect driven by it reads as flicker even though it is following the audio exactly. These follow
// the piece instead of the notes: they swell through a chorus and subside through a verse. Symmetric,
// so they rise and fall at the same rate.
//
// Pick a rung by what the effect is *for*, not by taste. Something that should hit on the beat wants
// g_music.y; something that should breathe with the music wants y or z here. Interpolating between
// two rungs is fine and is how to land between them.
float4 g_music_slow : register(c211); // x = 0.5 s, y = 1.5 s, z = 4 s, w = 10 s

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
