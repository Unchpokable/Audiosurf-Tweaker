# Making skies for Audiosurf with Audiosurf Tweaker

Audiosurf Tweaker can replace the game's sky with one of your own. Not just a different picture — a
**sky package**: a folder of shaders, textures, scripts and settings that the plugin loads while the
game is running, hot-reloads when you save a file, and exposes as a panel of sliders you can drag
mid-ride.

A sky can be a cube map you painted. It can be a pixel shader that computes the sky per pixel, sharp
at any resolution and costing no memory. It can have clouds placed by your own Lua. And it can
**react to the song that is playing** — the game already knows what the music is doing, and your
shader can read it.

This is the manual for all of that. You do not need to build anything; a sky is a folder you drop in
place.

```json
{
  "format": 1,
  "name": "My First Sky",
  "layers": [
    { "id": "sky", "kind": "fullsky", "shader": "shaders/mysky.hlsl" }
  ]
}
```

That, plus one `.hlsl` file, is a working sky package.

---

## The manual

Read these in order the first time. After that they stand alone.

| | |
|---|---|
| **[Getting started](skyboxes/getting-started.md)** | Where files go, the smallest sky that works, the Skybox tab, hot reload, shipping your sky as a single file. |
| **[The manifest](skyboxes/manifest.md)** | `Config.json` end to end: layers, lights, shared values, bindings, textures, and the sliders your sky offers. |
| **[Writing the shader](skyboxes/shaders.md)** | The constant contract, what the plugin uploads and what it must never touch, declaring your own knobs, and the pixel-size trick everything sharp depends on. |
| **[Reacting to the music](skyboxes/music.md)** | What the game knows about the playing song, what it cannot know, and how to drive an effect from it without turning it into a flicker. |
| **[Clouds and geometry](skyboxes/clouds.md)** | The sprite layer: placing clouds with Lua, baking their texture, and lighting them from the sky's own lights. |
| **[Cost and limits](skyboxes/limits.md)** | What a sky costs, how to measure it rather than guess, resolution scaling, and the traps that produce a black sky with no error message. |

---

## Which kind of sky do you want?

**A picture.** You have a cube map, a panorama, or six square images. No package needed at all — see
[Getting started § Just an image](skyboxes/getting-started.md#just-an-image). Costs memory, is as
sharp as the file you supply, and takes five minutes.

**A shader.** The sky is computed for every pixel from a program you write. Sharp at any resolution,
costs no memory, can move, and can react to the music. Costs GPU time every frame and costs you an
afternoon of HLSL. This is what the rest of this manual is mostly about.

**Both.** A package can hold several layers. Nothing stops a shader sky with a sprite cloud layer
over it, which is what the sky that ships with the Tweaker does.

---

## What a package looks like

```
My Sky.sky/
  Config.json          the manifest - the only required file
  shaders/
    mysky.hlsl         the sky's pixel shader
    clouds.vs.hlsl     a cloud layer brings its own pair
    clouds.ps.hlsl
  textures/
    noise.dds          anything your shaders sample
  scripts/
    clouds.lua         where the clouds go, and what they are painted with
```

The folder's name ends in `.sky` and is what the game shows. Everything inside is referenced from
`Config.json` by a path relative to the package root — nothing is found by convention, and nothing
outside the package can be reached.

**A package can also be a zip.** Zip that folder, name the result `My Sky.sky`, and it loads
identically — same settings, same everything. Ship the zip; work in the folder.

---

## A note on what is and is not stable

This is a beta. The manifest is `"format": 1` and the plugin refuses formats it does not know, so a
package that loads today will keep loading. Where something is likely to change, this manual says so
out loud rather than leaving you to find out.

The one thing to know up front: **registers `c208`–`c223` belong to the engine.** Everything else in
the shader constant file is yours. See [Writing the shader](skyboxes/shaders.md).
