# Cost and limits

[← back to the index](../skyboxes.md)

A procedural sky is a fullscreen pass with a large shader. It is the most expensive thing the
Tweaker draws, and it is drawn every frame. This page is about knowing what that costs rather than
guessing.

---

## The one number that matters

Audiosurf locks its frame rate to twice the monitor's refresh. That is not a smooth ceiling: miss
the budget and the frame rate drops to the *next* division, so 360 becomes 180 and the picture goes
visibly ragged. There is no gentle degradation to slide down.

So the question is never "is my sky fast" but "does it fit in the gap".

For scale, from the sky bundled with the Tweaker, measured in game at 2560×1440 with everything on:

| | |
|---|---|
| before any optimisation | ~3.0 ms |
| after baking one noise field to a texture | ~1.2 ms |
| the sprite cloud pass, of that | ~0.3 ms |

---

## Resolution scaling

The Skybox tab has a resolution slider. The sky is drawn at that fraction of the viewport and
stretched back up.

**67 % roughly halves the cost.** On most skies it is not visible — a sky is mostly large soft
gradients, which is exactly what survives an upscale. What does not survive: hard-edged features
near the pixel size, so a fine star field or a thin arc will soften.

Try it before optimising anything. It is one slider and it buys more than most shader work.

---

## What is actually expensive

In rough order, from the bundled sky's own measurements:

1. **Multi-octave noise per pixel.** By a wide margin. A layer running fifteen `noise3` calls per
   pixel cost 522 µs; the same layer sampling one baked octave from a volume texture cost 86 µs.
2. **Ray marches.** Anything with a loop whose length depends on the pixel.
3. **`atan2` and `acos` on the view direction.** Not free, but a rounding error next to the above.
4. **Everything else.** Gradients, discs, glows, `lerp` chains — effectively free at this scale.

The lesson from the first line: **bake the noise, keep the octaves**. Bake *one* octave of your
noise into a small volume texture and keep the octave loop in the shader. Do not try to bake the
whole fbm — the per-octave rotation has no lattice symmetry, so a multi-octave bake cannot tile, and
you will get a visible repeat instead of a saving.

If your noise needs its gradient, bake that too: RGB for the gradient, A for the value. It fits one
`tex3Dlod` and saves recomputing derivatives.

---

## Measure, do not reason

Frame rate is a terrible instrument here — vsync quantises it, and the overlay and the game move
around underneath you.

The right tool is a timer on the draw call itself, which the plugin has. Turn on the sky's timing
readout and change **one** thing at a time. Zero a knob rather than editing the shader: a knob at
zero takes a coherent branch and the layer is genuinely skipped, so what you measure is that layer's
cost and nothing else.

And measure variants **interleaved**, not one after the other. A GPU's clock drifts over tens of
seconds, and a straight A-then-B comparison will happily tell you that turning a layer *off* cost
you 40 µs.

---

## A black sky

In rough order of likelihood:

**A NaN in a constant.** This is the one that gives no error at all. Anything that divides by a
value which can be zero, any `pow` of a negative, any `normalize` of a zero vector — one NaN
propagates through the whole pixel and the sky goes black or garbage. Nothing logs it.

If the sky went black after an edit, comment out half of `main` and bisect. There is no faster way.

**A shader that did not compile.** Check the diagnostics panel. The previous shader stays up when a
recompile fails, so this shows up as "my edit did nothing" more often than as black.

**A register collision.** If you declared something in `c208`–`c223`, the engine's per-frame write
lands on top of it — or on top of a literal fxc placed there. Move it down.

---

## Hard limits

| | |
|---|---|
| Shader model | `ps_3_0` / `vs_3_0`. No compute, no tessellation, no unbounded loops. |
| Float constants | 224 (`c0`–`c223`), of which `c208`–`c223` are the engine's. |
| Samplers | 16 (`s0`–`s15`). |
| Texture format | `.dds` — 2D, volume and cube. The header decides which. |
| Dynamic flow control | Derivatives are undefined inside it. Use `tex2Dlod`/`tex3Dlod` and measure `fwidth` before you branch. |

---

## Things that will change

This is a beta. Where the format is likely to move:

- **The music interface will gain resolution, not names.** A finer FFT computed by the plugin is
  planned. It will arrive behind the existing `low`/`mid`/`high`/`air` and the slow ladder, so a sky
  written against those keeps working. A sky written against `g_music.x` — the game's raw total —
  is on its own.
- **Meshes.** A sprite is right for a cloud because a cloud has no definite shape. A monolith needs
  a silhouette with straight edges, and that will be its own layer kind rather than a generalised
  sprite.
- **`format: 1`** is stable. If it ever becomes 2, packages declaring 1 keep loading.

---

## Things that will not

- **Registers `c208`–`c223` are reserved permanently.** Do not use them.
- **A package can only read its own files.** Every path is resolved inside the package, and that is
  checked in one place rather than trusted.
- **Fixed seeds by default.** A generator that surprises you on every load has to ask for it.
