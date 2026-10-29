// The sky: one ps_3_0 pixel shader. It is handed the direction a pixel looks in and returns its colour.
// How to write one: https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/skyboxes/shaders.md
//
// sky_common.hlsli comes from the plugin - the game compiles this file against its own copy. The one in
// .tweaker/include is only there so your editor can find it.
#include "sky_common.hlsli"

// Your own knobs. c0-c5 are sky_common's palette and c208-c223 belong to the engine (the music block), so yours
// start at c6. Each component is a slider in Config.json ("g_sun.x" and so on).
float4 g_sun : register(c6); // x = disc radius (degrees), y = glow tightness, z = glow strength

static const float k_deg = 0.01745329;

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);

    // How wide one pixel is, in radians. Measured here, at the top, where every pixel takes the same path -
    // derivatives are undefined inside a branch. Anything thinner than a pixel is widened to one and dimmed by as
    // much, so it fades out instead of flickering (shaders.md, "Staying sharp").
    const float px = (fwidth(d.x) + fwidth(d.y) + fwidth(d.z)) * 0.35;

    // Zenith to horizon to ground, from the three palette colours.
    float3 colour = sky_gradient(d);

    // The sun. g_light.xyz is the direction to it and g_light_color its colour times its strength - both bound to
    // lights.sun in Config.json, so moving the sun's sliders moves it here.
    const float3 to_sun = normalize(g_light.xyz);
    const float facing = dot(d, to_sun);
    const float angle = acos(clamp(facing, -1.0, 1.0));

    const float authored = g_sun.x * k_deg;
    const float radius = max(authored, px);
    const float disc = (1.0 - smoothstep(radius - px, radius + px, angle)) * saturate(authored / radius);
    const float glow = pow(saturate(facing), g_sun.y) * g_sun.z;

    colour += g_light_color.rgb * (disc * 4.0 + glow);

    return float4(colour, 1.0);
}
