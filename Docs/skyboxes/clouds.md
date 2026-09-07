# Clouds and geometry

[← back to the index](../skyboxes.md)

A pixel shader is very good at a sky and quite bad at a cloud. A cloud has a *silhouette* — an edge
in a particular place, not a threshold on a noise field — and getting one out of a fullscreen shader
costs a march per pixel.

So there is a second kind of layer. `kind: "sprites"` puts actual quads on the sky sphere: your
script says where they go, your script (or the engine, or a file) says what they are painted with,
and the sky's own lights light them.

```json
{
  "id": "clouds",
  "kind": "sprites",
  "shader": "shaders/clouds",
  "generator": "scripts/clouds.lua",
  "fill": { "kind": "atlas", "generator": "scripts/clouds.lua", "tiles": 6, "size": 256 },
  "bind": {
    "g_light.xyz":  "lights.primary.direction",
    "g_light.w":    "lights.ambient.intensity",
    "g_light2.xyz": "lights.twin.direction",
    "g_light2.w":   "lights.twin.intensity"
  },
  "params": [ ... ]
}
```

`shader` is the **stem** of a pair: `shaders/clouds` reads `clouds.vs.hlsl` and `clouds.ps.hlsl`.
Leave it out and the layer draws with the pair built into the plugin.

---

## The division of labour

The engine owns the billboard basis, the pole handling and the vertex format. Those are invariants,
not opinions — get them wrong and the sprites face the wrong way or tear at the zenith.

Your script owns **where the clouds are**, which is the part that was only ever a matter of taste.

---

## `place()` — where the clouds go

The engine calls this whenever the layer is rebuilt: a knob moved, the sky was chosen, or you saved
the file.

```lua
function place()
    local count  = math.floor(tw.prop("count"))
    local clumps = math.max(1, math.floor(tw.prop("clumps")))
    local size   = math.tan(math.max(tw.prop("size"), 0.05) * DEG)

    for i = 0, count - 1 do
        local x, y, z = direction_for(i, count)
        local scale = size * (0.55 + 0.9 * tw.random())

        tw.emit(x, y, z, tw.random() * 2.0 * math.pi, scale, scale,
                math.floor(tw.random() * 4))
    end
end
```

| | |
|---|---|
| `tw.emit(x, y, z, roll, sx, sy, tile)` | One sprite: a direction, a roll in radians, half-extents, and which atlas tile. |
| `tw.emit_at(...)` | The same with an explicit basis, for when you want to control the orientation yourself. |
| `tw.prop(name)` | A native property of this layer — the manifest's `"prop"` params. Asking for one the manifest does not declare is an **error**, not a nil: `place()` runs only at build time, and a manifest out of step with its script should say so immediately. |
| `tw.seed()` | This layer's seed. See below. |
| `tw.random()` | Seeded from that. Deterministic. |
| `tw.lights` | Read-only table of the sky's lights, if placement should care where the sun is. |
| `tw.log(msg)` | Goes to the plugin's log. |

### Distributing clumps without visible structure

Worth knowing because the obvious answers both fail. A Fibonacci spiral is uniform and deterministic
and *also visibly a spiral* — the eye finds the arms at any count worth drawing. Independent random
directions clump and leave bald patches at the same counts.

What works is neither: a lattice whose points are let off their exact positions. Derive a cell
radius from the count and the band of sky being filled (N points sharing a solid angle
`2π(ceiling − floor)` get `πr²` each), then displace each point by up to that radius. At full
scatter a clump may land anywhere in its own cell, which reads as random and cannot leave a hole.

---

## `fill()` — what they are painted with

Declared in the manifest, not decided by the script, so "is this package complete" is a question you
can answer by reading it rather than by running it.

```json
"fill": { "kind": "atlas", "generator": "scripts/clouds.lua", "tiles": 6, "size": 256 }
```

| `kind` | |
|---|---|
| `builtin` | The plugin's own cloud bake. What a layer with no `fill` block gets. |
| `atlas` | Your script bakes the tiles. `fill()` is called and must hand back every one. |
| `texture` | A ready-made image shipped in the package — `"path": "textures/clouds.png"`. |
| `shader` | Nothing is baked; the pixel shader paints the sprite itself. |

An `atlas` fill looks like this:

```lua
function fill(tiles, size)
    for i = 0, tiles - 1 do
        local height = tw.layer(size, size)
        tw.fbm(height, { octaves = 5, frequency = 2.8, warp = 0.6,
                         seed = tw.seed() + i * 7919 })
        tw.radial(height, { inner = 0.60, feather = 0.36, depth = 0.50 })

        local tile = tw.layer(size, size)
        tw.normals(tile, height, { span = 3, relief = 1.4 })
        tw.alpha(tile, height)

        tw.tile(i, tile)
    end
end
```

The image operations available: `tw.layer` (a float RGBA buffer), `tw.load`, `tw.resize`,
`tw.composite`, `tw.fbm`, `tw.radial`, `tw.normalize`, `tw.normals`, `tw.alpha`, `tw.tile`.

`tw.normals` writing into RGB and `tw.alpha` into A is the usual arrangement: the sprite shader
wants a normal to light against and a coverage mask to cut the silhouette with.

---

## Seeds

```json
"seed": 12345      // or "random"
```

**Fixed by default**, and that is deliberate. With a fresh seed on every rebuild, "did my edit change
anything?" has no answer — you would be guessing at a generator instead of tuning one.

A sky that surprises you on every load should be something its author asked for out loud.

The choice lives in the manifest rather than in the script because the script's code is identical
either way — it always asks `tw.seed()` — so keeping it as data means it can be pinned while
somebody is working on the sky.

---

## Native properties

A `sprites` layer has knobs that no shader constant could express, because they decide what is *in*
the vertex buffer rather than how it is shaded. They use `"prop"` instead of `"var"`, and moving one
rebuilds the layer's geometry:

```json
{ "prop": "count",   "label": "Sprites",     "default": 220, "min": 0, "max": 900 },
{ "prop": "clumps",  "label": "Clumps",      "default": 22,  "min": 1, "max": 200 },
{ "prop": "spread",  "label": "Clump spread (deg)", "default": 12.0, "min": 0, "max": 60 },
{ "prop": "size",    "label": "Size (deg)",  "default": 4.0, "min": 0.5, "max": 20 },
{ "prop": "scatter", "label": "Scatter",     "default": 0.8, "min": 0, "max": 1 },
{ "prop": "floor",   "label": "Floor (deg)", "default": 5.0, "min": -20, "max": 60 },
{ "prop": "ceiling", "label": "Ceiling (deg)", "default": 55.0, "min": 0, "max": 90 }
```

Your script reads them with `tw.prop(name)`. You are free to invent your own — declare it in the
manifest and read it in the script; the two names must match.

---

## The sandbox

A generator script runs in its own VM, separate from the Tweaker's general scripting. It cannot
reach the filesystem, the network, the game's channel graph, or anything outside its package. There
is no `io`, no `require`, no `ffi`, no `loadstring`.

That is not a security boundary against a determined attacker — it is a guard rail that stops a sky
package you downloaded from doing anything except describe a sky.

Time is bounded too: a generator that loops forever is stopped rather than hanging the game.

---

## Lighting

Everything a sprite layer needs to be lit correctly comes through `bind`. Ask for
`lights.<id>.direction` and `lights.<id>.radiance` and the sky answers from its own declaration.

Neither side knows the other. The sky does not publish anything for the clouds to fetch, and the
clouds do not know that a bearing in degrees is how the sky's author prefers to write a direction
down. That is what makes a second or third light cost one line in the manifest instead of a new
adapter.

---

Next: **[Cost and limits](limits.md)**.
