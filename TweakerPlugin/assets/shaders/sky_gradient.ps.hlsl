// Clear daytime sky: gradient, sun, and a haze band hugging the horizon.
//
// The first sky program that is meant to be looked at rather than measured. Deliberately the
// simplest thing that still beats a photograph stretched over 512px faces - there is nothing here a
// cube map could not have stored, except that this version is sharp at any resolution and costs no
// memory at all.
//
// Uses c0..c4, not c5: nothing here moves, so sky_program lists it as five registers.
#include "sky_common.hlsli"

float4 main(sky_in input) : COLOR0
{
    float3 d = normalize(input.dir);

    float3 col = sky_gradient(d);
    col += light_term(d);

    // Aerial perspective, cheaply: air is thickest along the horizon, so light scatters into a warm
    // band there. exp() rather than a smoothstep because the falloff should be quick near y = 0 and
    // then linger - the same reason the gradient uses a fractional exponent.
    float haze = exp(-abs(d.y) * 6.0);
    col = lerp(col, g_horizon * 1.15, haze * 0.35);

    return float4(col, 1.0);
}
