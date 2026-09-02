// Pixel shader for the sky's sprite layer - Docs/Internal/skybox-geometry.md, phase 3.
//
// Density and a surface normal come from an atlas baked on the CPU (sky_sprite_atlas): the alpha
// gives each sprite a silhouette with lobes and gaps rather than a circle, and the normal makes the
// light break over that shape instead of gliding across a sphere.
//
// The two normals are combined rather than swapped. The sphere term is what makes a flat quad read
// as a body at all - it is the low frequency, the overall bulge - and the atlas normal is the high
// frequency lumps on top of it. Dropping either one is visible: without the sphere the sprite goes
// flat, without the atlas it goes smooth.

float4 g_light : register(c0);    // xyz: direction *towards* the first sun, w: ambient floor 0..1
float4 g_tint : register(c1);     // rgb: body colour, a: overall opacity
float4 g_material : register(c2); // x: bump, y: edge softness, z: sun-through strength, w: its sharpness
float4 g_light2 : register(c3);   // xyz: direction towards the second sun, w: its weight (0 = none)

sampler2D s_atlas : register(s0);

struct ps_in {
    float4 uv : TEXCOORD0;      // xy: -1..1 across the quad, zw: atlas coordinates
    float3 centre : TEXCOORD1;  // unit direction to the sprite's centre
    float3 tangent : TEXCOORD2; // baked billboard basis, carrying the sprite's own rotation
    float3 ray : TEXCOORD3;     // object-space position of this pixel; the view direction, unnormalised
};

// How much light a sun puts into this pixel: diffuse off the surface, plus the sun coming *through*
// the cloud when it stands between the viewer and that sun.
float lit_by(float3 direction, float3 normal, float3 view, float ambient)
{
    const float3 light_dir = normalize(direction);

    const float lambert = saturate(dot(normal, light_dir));

    // `view` points from the viewer out at the pixel and `light_dir` points at the sun, so this is
    // high exactly when the cloud is between the two - which is the case the term exists for.
    //
    // The first version had `-view` here and multiplied the result by (1 - density). Both were
    // wrong, and together they drew a hard white outline around every cloud with a dark body inside:
    // the sign lit clouds when the sun was behind the *camera*, and scaling by thinness put the
    // whole effect on the silhouette, since thinness is by definition greatest exactly there. Light
    // scattered towards the eye comes from the cloud's body, so it belongs on the body.
    const float through = pow(saturate(dot(view, light_dir)), max(g_material.w, 1.0)) * g_material.z;

    return ambient + (1.0 - ambient) * lambert + through;
}

float4 main(ps_in input) : COLOR
{
    const float4 atlas = tex2D(s_atlas, input.uv.zw);

    // Cheapest possible early out, and the common one: most of a quad's area is outside its cloud.
    clip(atlas.a - 0.004);

    const float r2 = dot(input.uv.xy, input.uv.xy);
    const float disc = saturate(1.0 - r2);

    const float3 axis = normalize(input.centre);
    const float3 t = normalize(input.tangent);
    const float3 b = cross(axis, t);

    // The sphere normal. `z` is the component pointing back along the line of sight, and on a
    // skybox the viewer sits at the origin looking outward - so the face turned towards them is
    // -axis, not +axis.
    const float3 sphere = t * input.uv.x + b * input.uv.y - axis * sqrt(disc);

    // The atlas normal arrives in the quad's own tangent frame, which is why the basis above has to
    // carry the sprite's rotation - otherwise the lighting would stay put while the shape turned.
    const float3 bump = atlas.rgb * 2.0 - 1.0;
    const float3 detail = t * bump.x + b * bump.y - axis * bump.z;

    const float3 normal = normalize(lerp(sphere, detail, saturate(g_material.x)));

    // Per pixel rather than per sprite. The sprite's centre would do for the diffuse term, but the
    // sun-through term falls off over a few degrees, and holding it constant across a sprite four
    // degrees wide makes each one a flat patch of brightness with a step at its neighbour.
    const float3 view = normalize(input.ray);

    float shade = lit_by(g_light.xyz, normal, view, g_light.w);

    // A second sun, weighted. A sky that has two and lights its clouds by one is visibly wrong from
    // half the angles - the clouds agree with one light and contradict the other.
    shade += lit_by(g_light2.xyz, normal, view, 0.0) * g_light2.w;

    // Edge softness fades the sprite out towards the boundary of its own quad, on top of whatever
    // the atlas already did, so a large sprite can never end in a straight line.
    const float edge = pow(disc, max(g_material.y, 0.01));

    const float alpha = atlas.a * edge * g_tint.a;

    // Straight alpha (SRCALPHA / INVSRCALPHA), and the shader does not premultiply.
    //
    // Everything the cloud sends towards the eye is scattered by the cloud's own material, so it
    // scales with how much material there is - which is what alpha means and what the hardware
    // multiplies by. An earlier version premultiplied here and left the sun-through term
    // unattenuated so it would "show through thin cloud"; that is exactly backwards, and it is what
    // produced the white outline.
    //
    // The sun genuinely showing through a thin edge needs no term at all: the sky is drawn first,
    // sun included, and survives this blend in proportion to (1 - alpha). That is also what lets the
    // aurora read through a cloud, and why the opacity knob is really "how much shows through".
    return float4(g_tint.rgb * shade, alpha);
}
