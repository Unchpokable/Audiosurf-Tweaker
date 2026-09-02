// Vertex shader for the sky's sprite layer. Compiled to vs_3_0 at build time (see
// src/resource/CMakeLists) - Docs/Internal/skybox-geometry.md, phase 2.
//
// It is a pass-through, and that is the point. On a skybox the view matrix carries no translation
// (sky_renderer::prepare zeroes it), so "turn the quad to face the camera" means "turn it to face
// the origin" - which never changes. The billboard basis is therefore baked into the vertex buffer
// once, on the CPU, and this shader has no orientation work to do at all: no per-frame CPU pass
// over the sprites, no basis reconstruction here.
//
// Baking also disposes of the pole problem for free. A tangent basis on a sphere has to degenerate
// somewhere (hairy ball theorem), and picking the reference axis per sprite on the CPU handles the
// two sprites where it matters instead of branching per pixel forever after.
//
// g_wvp arrives already transposed, for the reason sky_cube.vs.hlsl explains.
float4x4 g_wvp : register(c0);

// uv.xy is the -1..1 quad coordinate the disc and the sphere normal come from; uv.zw is where this
// corner lands in the atlas. Both are baked, including the sprite's own random rotation, which is
// folded into the tangent rather than applied here: the pixel shader lights the atlas normal in
// this basis, so the shape and the lighting have to turn together or the lumps would stay lit from
// a fixed direction while the cloud rotated under them.
struct vs_in {
    float3 pos : POSITION;      // baked corner position, same object space as the sky cube
    float4 uv : TEXCOORD0;      // xy: quad coordinate, zw: atlas coordinate
    float3 centre : TEXCOORD1;  // unit direction to the sprite's centre
    float3 tangent : TEXCOORD2; // baked billboard basis, first axis; the second is cross(centre, t)
};

struct vs_out {
    float4 pos : POSITION;
    float4 uv : TEXCOORD0;
    float3 centre : TEXCOORD1;
    float3 tangent : TEXCOORD2;

    // The object-space position, passed through so the pixel shader can take a view direction per
    // pixel instead of per sprite. On a skybox the viewer is at the origin, so the position *is* the
    // direction, up to a scale normalize() divides out.
    float3 ray : TEXCOORD3;
};

vs_out main(vs_in input)
{
    vs_out output;

    output.pos = mul(float4(input.pos, 1.0), g_wvp);
    output.uv = input.uv;
    output.centre = input.centre;
    output.tangent = input.tangent;
    output.ray = input.pos;

    return output;
}
