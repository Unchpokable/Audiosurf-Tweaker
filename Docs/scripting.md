# Scripting Audiosurf with Audiosurf Tweaker

Audiosurf Tweaker can run your own **Lua scripts inside the running game**. A script can read the
game's live state — your score, the track ahead, which blocks you have collected — draw its own HUD
over the game, react to things happening the instant they happen, and even switch pieces of the
game's own behaviour off.

This is the user manual for that. You do not need to know C++, you do not need to modify the game,
and you do not need to rebuild anything. A script is a text file you drop in a folder.

```lua
-- @name        My First Script
-- @description Shows the current score in the corner

local points = tw.float_ch("StatCollector", "Points")

tw.on_frame(function()
    local score = points:get()
    if score == nil then return end          -- not in a run yet

    local x, y = tw.hud.safe()
    tw.hud.text(x + 20, y + 20, string.format("Score: %.0f", score), tw.theme("text_primary"))
end)
```

Drop that in `scripts/` next to `TweakerPlugin.dll`, start a run, and the number is on your screen.

---

## The manual

Read these in order the first time. After that they stand alone.

| | |
|---|---|
| **[Getting started](scripting/getting-started.md)** | Where files go, your first script, the Scripts tab, turning scripts on and off, what happens when something breaks. |
| **[How Audiosurf is built](scripting/game-model.md)** | The important one. The game is a graph of *channels*, and everything else here only makes sense once that clicks. How to think about it, and how to find the piece you want. |
| **[Reading and writing the game](scripting/channels.md)** | Channel accessors, values, text, vectors, tables, and how to handle the fact that nothing is available immediately. |
| **[Reacting and intercepting](scripting/hooks.md)** | Running code when the game does something, and stopping the game from doing it. |
| **[Drawing](scripting/drawing.md)** | The HUD API: text, shapes, measurement, colours, and laying out against the overlay. |
| **[API reference](scripting/api-reference.md)** | Every function, in one place. |
| **[Limits, safety and performance](scripting/limits.md)** | What the environment deliberately cannot do, what it costs, and the rules that keep a script from ruining the game. |

## What this is not

It is worth being clear up front about the shape of the thing.

**This is not a modding API the game's author designed.** Audiosurf has no scripting hooks for third
parties. Everything here was recovered by reverse-engineering the game and is built on top of its
internals. That has two consequences you will feel:

- **Names and structures come from the game, not from us.** They are inconsistent, occasionally
  misspelled (`Do_ResovleLaneCrash` is real), and sometimes ambiguous. We surface them as they are
  rather than inventing a tidy façade that would hide what is actually happening.
- **A script can break the game if it tries.** The API will stop you making a wild call, but it
  cannot know that the number you just wrote into a channel is one the game will divide by. See
  [Limits, safety and performance](scripting/limits.md).

**This is not sandboxed against malicious scripts.** The environment removes the obvious ways to
reach the filesystem or arbitrary memory, and that is enough to make honest mistakes survivable. It
is not a security boundary. Running someone's script is trusting them, the same as running any other
program they sent you.

**Scripts are per-machine, not per-save.** Nothing a script does is uploaded, and nothing it does
affects online leaderboards — the game reports its own score, not yours.

## Getting help

- The example scripts that ship with the Tweaker are meant to be read. `traffic.lua` in particular is
  heavily commented and demonstrates most of the API.
- If a script cannot find something, it says so in the notification feed. Read
  [Getting started § When something goes wrong](scripting/getting-started.md#when-something-goes-wrong).
- The deep reverse-engineering notes live in `Docs/Internal/` — they are working notes rather than
  documentation, in Russian, and much rougher than this manual. But if you are hunting for a piece of
  the game nobody has written up yet, that is where the map is.
