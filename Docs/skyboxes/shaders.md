# Writing the shader

[← back to the index](../skyboxes.md)

A `fullsky` layer is one ps_3_0 pixel shader. It is handed a view direction and returns a colour.
That is the whole interface.

```hlsl
#include "sky_common.hlsli"

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);
    return float4(sky_gradient(d), 1.0);
}
```

`sky_common.hlsli` comes from the plugin — do not ship a copy, and do not worry about where it is.
The `#include` is resolved against the plugin's own packed headers first, then next to your shader.

Shaders are compiled **in the running game**, from your source, every time you save. You never run
`fxc` yourself.

---

## What the plugin gives you

`sky_common.hlsli` declares the shared palette at `c0`–`c5`:

```hlsl
float3 g_zenith  : register(c0);
float3 g_horizon : register(c1);
float3 g_ground  : register(c2);
float4 g_light   : register(c3);       // xyz = direction, w = cos(angular radius)
float4 g_light_color : register(c4);   // rgb = colour, w = glow exponent
float4 g_runtime : register(c5);       // x = seconds, y = program-specific
```

plus two helpers — `sky_gradient(d)` for a zenith-to-ground ramp and `light_term(d)` for a disc with
a glow — and the music block, which has [its own page](music.md).

**Use as much or as little as you like.** The plugin reads your compiled shader's own constant table
and uploads exactly the registers it declares. A shader that reads nothing but `g_runtime` gets
`g_runtime` and nothing else is touched.

That is not an optimisation. Read the next section.

---

## The one rule: only declared registers are written

A shader model 3 shader carries **its own literals inside the bytecode**, as `def cN, …`
instructions, in the *same* register file the application writes to. They are loaded when the shader
is bound.

So writing a register your shader did not declare does not misconfigure the program. It **corrupts**
it — one of its constants is now something else, and the failure looks like a rendering bug with no
error anywhere.

The plugin therefore uploads only contiguous runs of registers your constant table actually
declares, and never the gaps between them. You do not have to do anything for this to work. It
matters to you for one reason:

**Registers `c208`–`c223` belong to the engine.** Do not declare anything there. Everything from
`c0` to `c207` is yours — 208 float4s, which no sky has come close to needing.

The diagnostics panel prints the runs your shader declared (`c0-c2, c5-c21, c24-c42`). If a knob is
not working, that listing is the first thing to look at: a variable fxc removed is not in it.

---

## Declaring your own knobs

Declare a uniform, then point a `params` entry at it in `Config.json`:

```hlsl
float4 g_haze : register(c32);   // x = density, y = coverage, z = detail
```

```json
{ "var": "g_haze.x", "label": "Density", "default": 0.4, "min": 0, "max": 1 }
```

Pack related things into one `float4` and comment what each component is. Registers are cheap but
the comment is what you will need in three months.

Two things that bite:

- **fxc drops what you do not read.** If `g_haze.z` never survives to the output, the whole variable
  may not be in the constant table, and the slider silently does nothing.
- **`@sky` annotations in the shader are not read by the plugin for a package layer.** They are for
  the offline tools. The plugin reads `Config.json`. Keep both in step or use only the manifest.

---

## Staying sharp

This is the part that separates a sky that looks right from one that shimmers, and it is not
obvious.

A pixel does not sample a point — it covers an angle. Any feature thinner than that angle will
alias: a star field becomes a field of flickering sub-pixel samples, a thin arc becomes a ragged
one-pixel line that crawls as the camera turns.

Measure the pixel once, at the top of `main`, and pass it down:

```hlsl
struct sky_frame {
    float3 d;
    float  px;   // angular size of a pixel, radians
    float  t;
};

f.px = (fwidth(d.x) + fwidth(d.y) + fwidth(d.z)) * 0.35;
```

Then anything thin gets a floor of one pixel **and is dimmed by however much the floor widened it**:

```hlsl
const float authored = width_deg * k_deg;
const float width    = max(authored, f.px);
const float shrink   = saturate(authored / max(width, 1e-6));

const float core = smoothstep(width, 0.0, distance) * shrink;
```

A feature narrower than a pixel then *fades out* instead of breaking up. That is the trade the star
field, the arcs and the cloud edges in the bundled sky all make, for the same reason.

**Derivatives are undefined inside dynamic flow control**, which is why `px` is measured at the top
where the flow is still straight, and everything below only reads it. Calling `fwidth` inside an
`if` that different pixels take differently is a bug the compiler will not always catch.

---

## Branching is free when it is coherent

`[branch]` on a **uniform** costs nothing — every pixel takes the same side, so the wave never
diverges. Use it to skip whole layers a knob has turned off:

```hlsl
[branch] if(g_aurora.x > 0.001)
{
    scene += aurora(f);
}
```

Branching on the view direction is also worth it when the split is spatial rather than scattered —
a wave of pixels entirely above the horizon really does skip the ground half. Branching on noise is
not: the wave takes both sides and you have paid for the branch as well.

---

## Time

`g_runtime.x` is seconds since the sky started drawing. Multiply it by a knob so the player can slow
your sky down or stop it:

```hlsl
f.t = g_runtime.x * g_time_scale;
```

There is no frame counter and no delta time. Anything that needs to *accumulate* over frames cannot
be done in the shader — it has no state. If you need an envelope over time, the music block already
provides several; see [Reacting to the music](music.md).

---

## Textures

Declare the sampler, and tell the manifest which slot it is:

```hlsl
sampler3D s_noise : register(s0);
```

```json
"textures": [ { "path": "textures/noise.dds", "slot": 0, "address": "wrap" } ]
```

2D, volume and cube all work; the `.dds` header says which.

One ps_3_0 restriction that will find you: **inside dynamic flow control you must use an explicit
LOD**. `tex3D` computes derivatives, and the compiler will refuse with
`X3528: can't force branch with gradients on non-inputs`. Use `tex3Dlod` with level 0:

```hlsl
const float4 t = tex3Dlod(s_noise, float4(p * scale, 0.0));
```

A baked texture is often much cheaper than the maths it replaces — in the bundled sky, one octave of
value noise baked to a 128³ volume cut a layer from 522 µs to 86 µs. Bake **one octave**, not the
whole fbm, and keep the octave loop in the shader: the per-octave rotation has no lattice symmetry,
so a multi-octave bake cannot tile.

---

## Vertex shaders

A `fullsky` layer does not need one — they all paint the same cube, and the plugin supplies it. A
`sprites` layer may bring its own; see [Clouds and geometry](clouds.md).

---

Next: **[Reacting to the music](music.md)**.
