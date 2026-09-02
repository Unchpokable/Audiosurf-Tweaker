// Vertex shader for this sky's cloud layer, compiled from here at run time. The manifest names it
// as "shaders/clouds", and the plugin compiles "<name>.vs.hlsl" and "<name>.ps.hlsl" as the pair.
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
// # Motion
//
// The billboard basis stays baked; what moves is where each sprite sits and how its quad is shaped,
// both computed here from the frame's time. That split is the point: the CPU decides the layout once
// per rebuild, and this decides where that layout *is* right now, at no CPU cost per frame.
//
// One structural rule the knobs are built around: **the secular motion is common to every sprite,
// and everything per-sprite is bounded.** Give each sprite its own drift rate and a three-minute song
// smears every clump into an even wash - which is exactly the distribution the placement went to
// trouble to avoid. So `drift` turns the whole layer as one, and wobble, breathe and shear are
// oscillations that never accumulate.
//
// The second thing worth knowing: the atlas is baked. Deformation stretches a fixed image rather
// than evolving a density field, so a little reads as life and a lot reads as pumping. The ranges in
// Config.json are chosen for the first.
//
// g_wvp arrives already transposed, for the reason sky_cube.vs.hlsl explains. It is at c0 by
// structure, not by convention - a vertex shader that took the transform somewhere else would not
// draw at all - and it is the one thing here the manifest cannot name. Everything below it is named.
float4x4 g_wvp : register(c0);

// x: seconds since the layer first drew. Written by the engine, which finds this variable by name.
float4 g_time;

// x: drift, degrees per second the whole layer turns about the sky's up axis - the wind.
// y: wobble amplitude in degrees; a bounded per-sprite circling, so clumps stay clumps.
// z: wobble rate, cycles per second.
// w: breathe, how much a sprite's quad swells and shrinks, 0..1.
float4 g_motion;

// x: breathe rate, cycles per second.
// y: shear, how far the quad leans; the lighting is computed in the unsheared basis, so keep it
//    small or the lumps stop agreeing with the silhouette.
// z: shear rate, cycles per second.
float4 g_motion2;

// A stable per-sprite number in 0..1, from the one thing every corner of a sprite shares and no two
// sprites do: the direction to its centre. Cheaper than a vertex attribute and it cannot fall out of
// step with one, because there is nothing to keep in step.
float sprite_phase(float3 centre)
{
    return frac(sin(dot(centre, float3(12.9898, 78.233, 37.719))) * 43758.5453);
}

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
    const float t = g_time.x;
    const float phase = sprite_phase(input.centre) * 6.2831853;

    float3 centre = input.centre;
    float3 tangent = input.tangent;
    float3 bitangent = cross(centre, tangent);

    // The corner, as an offset from the centre in that basis. Recovered rather than passed: the two
    // dot products cost less than a wider vertex and cannot disagree with the position they came
    // from, since they *are* the position they came from.
    float3 offset = input.pos - centre;
    float along = dot(offset, tangent);
    float across = dot(offset, bitangent);

    // Bounded, per-sprite, and therefore safe to give everyone a different phase: nothing here
    // accumulates, so two neighbours in a clump breathe out of step without ever drifting apart.
    const float breathe = 1.0 + g_motion.w * sin(phase + t * g_motion2.x * 6.2831853);
    const float shear = g_motion2.y * sin(phase * 1.7 + t * g_motion2.z * 6.2831853);

    along *= breathe;
    across *= breathe;
    along += shear * across;

    // Wobble moves the sprite, not its quad: the whole billboard circles a little. Its radius is an
    // angle, so a small tangent-plane displacement is all it takes.
    const float wobble = radians(g_motion.y);
    const float wobble_t = t * g_motion.z * 6.2831853;
    centre = normalize(centre + (tangent * cos(phase + wobble_t) + bitangent * sin(phase + wobble_t)) * tan(wobble));

    // Re-orthogonalise against the moved centre. The wobble is small, so this is a nudge - but the
    // pixel shader rebuilds the bitangent as cross(centre, tangent), and a tangent that has drifted
    // off the tangent plane would tilt the whole normal field rather than any one lump.
    tangent = normalize(tangent - centre * dot(centre, tangent));
    bitangent = cross(centre, tangent);

    float3 world = centre + tangent * along + bitangent * across;

    // The wind: one rotation about the sky's up axis, identical for every sprite, so a clump stays a
    // clump however long the song runs. Applied last, to the centre and the basis alike, because the
    // pixel shader lights the atlas normal in that basis and turning one without the other would
    // leave every cloud lit from a direction it is no longer facing.
    const float spin = radians(g_motion.x) * t;
    const float cs = cos(spin);
    const float sn = sin(spin);

    const float3x3 wind = float3x3(cs, 0.0, -sn, 0.0, 1.0, 0.0, sn, 0.0, cs);

    world = mul(world, wind);
    centre = mul(centre, wind);
    tangent = mul(tangent, wind);

    vs_out output;

    output.pos = mul(float4(world, 1.0), g_wvp);
    output.uv = input.uv;
    output.centre = centre;
    output.tangent = tangent;
    output.ray = world;

    return output;
}
