# Getting started

[← back to the index](../skyboxes.md)

---

## Where things live

The plugin writes its own settings file next to itself, named after the DLL:

```
TweakerUI/
  TweakerPlugin.dll
  TweakerPlugin.skybox.cfg     <- created on first run
  Skies/
    My Sky.json                <- one file per sky, holding whatever you moved
```

`TweakerPlugin.skybox.cfg` is a plain `key=value` file with comments, written with its defaults on
first run. The key that matters first is `skybox_dir` — the folder the Skybox tab browses. It starts
empty, so set it to wherever you want to keep your skies:

```
skybox_dir=Skyboxes
```

Relative paths are tried against three roots, in order: next to `TweakerPlugin.dll`, the game's
working directory (`engine/`), then the game root one above it. If none of them contains what you
named, the plugin's log lists every path it tried — that listing is there because "it does not work
wherever I put it" cannot be answered without it. An absolute path sidesteps the whole question.

Your sky goes in that folder as a subfolder named `Something.sky`.

---

## Just an image

If all you want is different art, you do not need a package at all.

Put a cube map next to the game and point the config at it:

```
skybox_file=Skyboxes/my-cubemap.png
```

That accepts a **cross image** (the familiar unfolded-cube layout) or a **folder of six square
faces** named `posx`/`negx`/`posy`/`negy`/`posz`/`negz`, or `px`/`nx`/…, or
`right`/`left`/`top`/`bottom`/`front`/`back`. Radiance `.hdr` works too, with `hdr_exposure` to
brighten it.

Faces smaller than `min_face_size` are upscaled with Catmull-Rom. That improves the reconstruction
filter — it invents no detail, and it costs `(target/source)²` memory, so leave it at 0 unless you
can see the difference.

Everything from here on is about the other kind of sky.

---

## The smallest package that works

Make a folder called `Test.sky` inside your `skybox_dir`, with two files in it.

**`Config.json`**

```json
{
  "format": 1,
  "name": "Test",
  "layers": [
    {
      "id": "sky",
      "kind": "fullsky",
      "shader": "shaders/test.hlsl"
    }
  ]
}
```

**`shaders/test.hlsl`**

```hlsl
#include "sky_common.hlsli"

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);
    return float4(sky_gradient(d), 1.0);
}
```

That is a complete sky. `sky_common.hlsli` comes from the plugin — you do not ship a copy — and
`sky_gradient` is one of two helpers in it that turn a view direction into a colour.

Open the overlay, go to the **Skybox** tab, and `Test` is in the list.

---

## The Skybox tab

Picking a sky compiles it and swaps it in immediately. Alongside the list you get:

- **Resolution.** The sky can be drawn at a fraction of the viewport and stretched back up. At 67 %
  a procedural sky costs roughly half as much and, on most skies, looks the same — see
  [Cost and limits](limits.md).
- **A parameter panel.** Every knob your manifest declares, grouped under the headings you gave
  them, live while the game runs.
- **Diagnostics.** If a shader failed to compile, the compiler's message is here rather than in a
  log file you have to go find.

Values you move are saved to `Skies/<your sky>.json`, keyed per sky. Deleting that file resets the
sky to its authored defaults; deleting the sky does not leave settings behind for a sky that no
longer exists.

---

## Hot reload

**Save a file and the sky rebuilds.** That applies to the shaders, the Lua generators, the textures
and the manifest itself. There is no reload button and no need to restart the game.

A shader that fails to compile **keeps the sky it had**. You get the error in the diagnostics panel
and the previous picture stays up, so a typo mid-edit does not black out your screen.

This is the whole authoring loop: game on one monitor, editor on the other, save.

---

## Shipping it

Zip the folder. Name the zip `My Sky.sky`. Done.

An archived package loads through exactly the same path as a folder — same manifest, same shaders
compiled at runtime, same settings file, same name in the list. Nothing downstream knows or cares
which form the bytes came from.

What that means in practice:

- **Work in a folder**, because hot reload watches files.
- **Ship the zip**, because it is one file and cannot arrive with half its contents missing.

One rule the format enforces either way: **a package can only read its own files.** Every path in
the manifest and every path a script asks for is resolved inside the package. There is no way to
write `../../..` and reach the player's disk, and that is checked in one place rather than trusted.

---

## When something does not work

In rough order of how often it is the answer:

1. **The sky is not in the list.** `skybox_dir` is not pointing where you think. The log lists the
   paths that were tried.
2. **The sky is in the list and is black.** Look at the diagnostics panel — a shader that failed to
   compile says so there. If it compiled, see the traps in [Cost and limits](limits.md#a-black-sky);
   a NaN in a constant paints black without a word.
3. **A slider does nothing.** The variable it names is not in the compiled shader. fxc removes a
   uniform the shader does not actually read, and a knob pointing at one that was removed has
   nowhere to write. See [The manifest § params](manifest.md#params).
4. **The sky looks right in a screenshot and wrong in motion.** Almost always the pixel-size term —
   see [Writing the shader § Staying sharp](shaders.md#staying-sharp).

---

Next: **[The manifest](manifest.md)**.
