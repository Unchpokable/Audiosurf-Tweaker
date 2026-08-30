// Night sky: dark gradient, a moon, and a procedurally placed star field that twinkles.
//
// This one is the argument for the whole approach. Stars are the worst possible case for an image
// based sky - single bright pixels against a dark field, exactly what a 9x magnified cube map turns
// into grey smudges - and here they cost forty instructions and stay pin-sharp at any resolution,
// with no texture and no memory.
//
// Uses c0..c5: g_runtime.x drives the twinkle, so sky_program lists it as six registers.
#include "sky_common.hlsli"

/* @sky
name  = Starry Night
group = Stars
param = g_stars.x | Star density    | 0.07 | 0.005 | 0.30
param = g_stars.y | Star brightness | 3.00 | 0.0   | 8.0
*/

// c6, above the palette sky_common.hlsli owns. The plugin reads the compiled shader's constant
// table and uploads only what it finds, so a program is free to declare whatever it needs up here.
float4 g_stars : register(c6); // x = fraction of cells holding a star, y = brightness

// One float out of a cell coordinate. Cheap rather than statistically pretty: the input is always
// an integer lattice point, and all that matters is that neighbouring cells land far apart.
float hash31(float3 p)
{
    p = frac(p * 0.3183099 + float3(0.71, 0.113, 0.419));
    p += dot(p, p.yzx + 19.19);
    return frac((p.x + p.y) * p.z);
}

// Stars on a lattice in direction space. One hash lookup per cell and no neighbour sampling: the
// star is placed well inside its own cell and its falloff is tight enough never to reach the
// boundary, which is what makes the usual 27-cell neighbourhood unnecessary.
float3 star_field(float3 d)
{
    float3 p = d * 110.0;
    float3 cell = floor(p);
    float3 f = p - cell;

    float h = hash31(cell);

    // Written as a fraction rather than as the threshold it becomes, so that turning the knob up
    // gives more stars - a control that runs backwards is a control nobody trusts.
    float present = step(1.0 - g_stars.x, h);

    float3 centre = 0.35 + 0.3 * float3(hash31(cell + 11.0), hash31(cell + 23.0), hash31(cell + 37.0));

    float star = saturate(1.0 - length(f - centre) * 12.0);
    star = star * star * star;

    float twinkle = 0.75 + 0.25 * sin(g_runtime.x * 2.7 + h * 43.0);
    float3 tint = lerp(float3(0.75, 0.85, 1.0), float3(1.0, 0.9, 0.75), hash31(cell + 5.0));

    return tint * (present * star * twinkle * g_stars.y);
}

float4 main(sky_in input) : COLOR0
{
    float3 d = normalize(input.dir);

    float3 col = sky_gradient(d);
    col += light_term(d);

    // Thinned out towards the horizon, where in life they are lost to haze and to whatever is on
    // the ground - and where, in Audiosurf, the track and its neighbours are.
    col += star_field(d) * saturate(d.y * 3.0);

    return float4(col, 1.0);
}
