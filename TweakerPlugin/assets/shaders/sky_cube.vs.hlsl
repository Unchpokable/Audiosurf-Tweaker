// Vertex shader for the skybox cube. Compiled to vs_3_0 at build time (see src/resource/CMakeLists).
//
// Exists because ps_3_0 may not be paired with the fixed-function vertex pipeline: D3D9 requires a
// vs_3_0 alongside it. It is otherwise the same eight-vertex cube the fixed-function path draws.
//
// g_wvp arrives already transposed. HLSL packs a float4x4 column-major by default, so mul(v, M)
// reads register c[i] as column i - which is what the CPU-side transpose produces from a row-major
// D3DMATRIX. See sky_renderer's upload.
float4x4 g_wvp : register(c0);

struct vs_in {
    float3 pos : POSITION;
};

struct vs_out {
    float4 pos : POSITION;
    float3 dir : TEXCOORD0;
};

vs_out main(vs_in input)
{
    vs_out output;

    output.pos = mul(float4(input.pos, 1.0), g_wvp);

    // Object-space position doubles as the view direction, exactly as it does for the cube map
    // lookup in the fixed-function path: on a cube centred at the camera the two differ only by a
    // positive scale, which normalize() divides out. The orientation matrix rotates the geometry,
    // so this direction is in *sky* space - which is what makes the axis markers in the probe
    // shader meaningful.
    output.dir = input.pos;

    return output;
}
