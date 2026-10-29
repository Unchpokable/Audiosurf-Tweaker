// A sky that breathes with the song: a glow along the horizon swells with the music.
// Writing the shader: https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/skyboxes/shaders.md
// The music block: https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/skyboxes/music.md
//
// sky_common.hlsli comes from the plugin - the game compiles this file against its own copy. The one in
// .tweaker/include is only there so your editor can find it.
#include "sky_common.hlsli"

// Your own knobs. c0-c5 are sky_common's palette and c208-c223 belong to the engine (the music block), so yours
// start at c6.
float4 g_sun : register(c6);         // x = disc radius (degrees), y = glow tightness, z = glow strength
float4 g_pulse : register(c7);       // x = response, y = 0 beat .. 1 phrase, z = band height (degrees)
float3 g_pulse_color : register(c8); // the glow's colour

static const float k_deg = 0.01745329;

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);
    const float px = (fwidth(d.x) + fwidth(d.y) + fwidth(d.z)) * 0.35;

    float3 colour = sky_gradient(d);

    // The sun, as in the plain gradient sky.
    const float3 to_sun = normalize(g_light.xyz);
    const float facing = dot(d, to_sun);
    const float angle = acos(clamp(facing, -1.0, 1.0));
    const float authored = g_sun.x * k_deg;
    const float radius = max(authored, px);
    const float disc = (1.0 - smoothstep(radius - px, radius + px, angle)) * saturate(authored / radius);
    colour += g_light_color.rgb * (disc * 4.0 + pow(saturate(facing), g_sun.y) * g_sun.z);

    // How loud the music is right now. g_music.y follows notes, g_music_slow.y (1.5 s) follows the phrase - on busy
    // music the first reads as flicker, so the player picks a point between them. Every value here is already
    // smoothed: a shader has no memory and could not filter it itself.
    //
    // Always multiply by g_music.w. It is 0 in menus and between songs, and without it the sky would freeze on
    // whatever it last heard instead of settling.
    const float loudness = lerp(g_music.y, g_music_slow.y, saturate(g_pulse.y));
    const float drive = loudness * g_music.w * g_pulse.x;

    // A band hugging the horizon, fading with height above and below it.
    const float elevation = asin(clamp(d.y, -1.0, 1.0)) / k_deg;
    const float band = exp(-abs(elevation) / max(g_pulse.z, 0.5));

    colour += g_pulse_color * band * drive;

    return float4(colour, 1.0);
}
