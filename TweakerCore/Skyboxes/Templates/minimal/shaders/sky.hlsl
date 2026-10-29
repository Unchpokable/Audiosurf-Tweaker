#include "sky_common.hlsli"

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);
    return float4(sky_gradient(d), 1.0);
}
