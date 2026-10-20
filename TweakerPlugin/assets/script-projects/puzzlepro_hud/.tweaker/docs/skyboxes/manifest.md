# The manifest

[← back to the index](../skyboxes.md)

`Config.json` is the only file a package must have. It says what the sky is made of, what it is lit
by, and what the player is allowed to move.

**Comments are allowed**, and so are trailing commas. `//` and `/* */` both work, and the sky that
ships with the Tweaker uses them heavily. A manifest is a document, not a data dump.

---

## The shape of it

```json
{
  "format": 1,
  "name": "My Sky",
  "author": "Вася",
  "version": 1,

  "shared": {
    "lights": [ ... ],
    "values": [ ... ]
  },

  "layers": [ ... ]
}
```

`format` is the manifest generation and is currently `1`. The plugin refuses a format it does not
know rather than guessing at it.

`version` is **your** layout generation, and it exists for one purpose: it is folded into the key
your settings are saved under. Bump it when you rearrange what your registers mean, so a player's
saved values do not get applied to knobs that now mean something else. Do **not** bump it for
ordinary edits — that throws away everything they tuned.

---

## Layers

A sky is a list of layers, drawn in the order they are declared.

```json
"layers": [
  {
    "id": "sky",
    "kind": "fullsky",
    "shader": "shaders/mysky.hlsl",
    "enabled": true
  }
]
```

| field | meaning |
|---|---|
| `id` | Unique within the sky. It is the settings key and what diagnostics name. |
| `kind` | `fullsky` — a pixel shader painting the whole cube. `sprites` — the geometry layer, see [Clouds](clouds.md). |
| `shader` | Path from the package root. For `sprites` it is the **stem** of a `.vs`/`.ps` pair: `"shaders/clouds"` reads `clouds.vs.hlsl` and `clouds.ps.hlsl`. May be omitted on a sprite layer, which then uses the pair built into the plugin. |
| `enabled` | Default true. A layer switched off costs nothing. |
| `textures` | What this layer's shader samples — below. |
| `bind` | Where the sky's shared values go — below. |
| `params` | The knobs this layer offers — below. |

Sprite layers take three more: `generator`, `fill` and `seed`. Those are all in
[Clouds and geometry](clouds.md).

---

## Lights

Every layer binds to the sky's lights rather than declaring its own. That is the point of the
format: the clouds and the sky agree about where the light is coming from because there is one
answer, not two that have to be kept in step by hand.

```json
"shared": {
  "lights": [
    {
      "id": "primary",
      "bearing":   { "label": "Bearing (deg)",   "default": -14.5, "min": -180, "max": 180 },
      "elevation": { "label": "Elevation (deg)", "default": 29.0,  "min": -20,  "max": 70 },
      "color":     { "label": "Sun colour",      "default": [1.0, 0.96, 0.86] },
      "intensity": { "label": "Sun strength",    "default": 1.0,   "min": 0, "max": 2 }
    },
    {
      "id": "twin",
      "bearing":   { "label": "Twin offset (deg)", "default": 180, "min": 0, "max": 180,
                     "relative_to": "primary" },
      "elevation": { "same_as": "primary" },
      "color":     { "default": [0.72, 0.82, 1.0] }
    },
    {
      "id": "ambient",
      "kind": "ambient",
      "intensity": { "label": "Ambient", "default": 0.25, "min": 0, "max": 1 }
    }
  ]
}
```

`kind` is `directional` (the default) or `ambient`. A light is a direction, a colour and an
intensity, and **any of the three may be left out** — a light with no colour is white, one with no
intensity is at full strength. Nothing here is specific to a sun: a moon, an aurora, or the glow off
a planet filling half the sky is declared identically.

`relative_to` and `same_as` are for the second light, which in practice is almost always authored as
an offset from the first rather than with its own absolute bearing. The chain is resolved for you.

**A light may carry fields you invented.** `"sun_oreol_radius": { … }` alongside the four above is an
ordinary knob, bound and read by the same paths. It belongs to the light, so a sky with two suns
gets two halo radii without you naming them `primary_halo` and `twin_halo` by hand. The names in the
table below are reserved and yours may not shadow them.

---

## Shared values

Loose numbers several layers need to agree about.

```json
"values": [
  { "id": "haze", "label": "Haze", "default": 0.4, "min": 0, "max": 1 }
]
```

Use one when the same number appears in two layers. Use a layer's own `params` when it does not.

---

## Bindings

`bind` puts a shared value into one of your shader's uniforms.

```json
"bind": {
  "g_light.xyz": "lights.primary.direction",
  "g_light.w":   "lights.ambient.intensity",
  "g_sun.x":     "lights.primary.bearing",
  "g_haze.z":    "values.haze"
}
```

The left side names a variable and a swizzle in **your** shader. The right side is one of:

| source | what you get |
|---|---|
| `lights.<id>.bearing` | degrees, as authored — an offset for a relative light |
| `lights.<id>.elevation` | degrees; a `same_as` light resolves to the one it points at |
| `lights.<id>.color` | rgb, white when the light declares none |
| `lights.<id>.intensity` | scalar, 1 when the light declares none |
| `lights.<id>.direction` | **derived**: the unit vector, from the *absolute* bearing and elevation |
| `lights.<id>.radiance` | **derived**: colour × intensity — what a lighting shader actually wants |
| `values.<id>` | a shared value |

The two derived forms are why this is worth having. Your sky shader can ask for degrees because that
is what its own maths wants, while your cloud shader asks the same light for a unit vector — one
source, two forms, and no adapter between them.

`suns.` is accepted as a synonym for `lights.` and `.strength` for `.intensity`; both are leftovers
from the first packages and are not the spelling to use in new ones.

---

## Textures

```json
"textures": [
  {
    "name": "noise",
    "path": "textures/noise_p8_n128.dds",
    "slot": 0,
    "address": "wrap",
    "filter": "linear"
  }
]
```

`.dds` only, and **2D, volume and cube all work**. Which one it is comes from the file's own header —
the manifest deliberately does not say, because a second source of truth is a second thing that can
disagree with the first.

`slot` is the sampler register your shader declares: `sampler3D s_noise : register(s0)` is slot 0.
**It is a number, not a name, and nothing checks it for you** — ps_3_0 bytecode carries no sampler
names to match against. Get it wrong and the texture is bound where the shader is not looking.

`address` is `wrap` (default), `clamp` or `mirror`. `filter` is `linear` (default) or `point`. Both
are sampler state, which is why they are declared here rather than baked into the file: a tiling
noise field wants `wrap`, a gradient ramp wants `clamp`, and the same file could be either.

---

## Params

The sliders your sky offers, grouped under headings.

```json
"params": [
  {
    "group": "Horizon",
    "params": [
      { "var": "g_blend.x", "label": "Split (deg)", "default": 12.0, "min": 0, "max": 90 },
      { "var": "g_zenith",  "label": "Zenith",      "default": [0.10, 0.28, 0.62] }
    ]
  }
]
```

| field | meaning |
|---|---|
| `var` | A shader variable and optional swizzle. Three defaults make it a colour picker; one makes it a slider. |
| `prop` | Instead of `var`: a **native property** of a sprite layer — see [Clouds](clouds.md). Moving one rebuilds the layer's geometry rather than changing a constant. |
| `label` | What the panel shows. |
| `id` | The settings key. Defaults to the variable name; set it explicitly if you rename a variable and want saved values to follow. |
| `default`, `min`, `max` | Self-explanatory. `min`/`max` are ignored for colours. |

### The trap worth reading twice

A shader can also carry an `@sky` annotation block in its own source, and the offline tools read it.
**The plugin does not read it for a package layer** — it reads `params` from `Config.json`.

So a knob that exists only as an `@sky` line is a knob that does nothing in game, and it fails
silently: the register keeps whatever it had, which is usually zero, and the effect it controls
simply never appears. If a slider is missing from the panel or an effect never shows up, check that
it is in `Config.json`.

The other half of the same trap: **fxc drops a uniform your shader does not actually read.** If a
variable is only referenced in a way the compiler can fold away, it is not in the compiled constant
table, and a param naming it has nowhere to write. The diagnostics panel lists the registers the
shader really declared.

---

Next: **[Writing the shader](shaders.md)**.
