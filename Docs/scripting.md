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

Drop that in `engine\TweakerStuff\Scripts\` inside your Audiosurf install, start a run, and the
number is on your screen. No rebuild, and Audiosurf Tweaker does not have to be running.

When you are ready to write something of your own, the shortest way in is the **Script Dev** tab of
Audiosurf Tweaker: it creates a project, opens it in your editor, builds it and installs it into the
game — see [Scripts in Audiosurf Tweaker](scripting/tweaker.md).

This manual ships with Audiosurf Tweaker too: `DevTools\Docs\` next to `TweakerUI.exe` holds it, the
[sky packages](skyboxes.md) manual and [How Audiosurf works inside](internals.md), for reading offline.

---

## The manual

Read these in order the first time. After that they stand alone.

| | |
|---|---|
| **[Getting started](scripting/getting-started.md)** | Where files go, your first script, the Scripts tab, turning scripts on and off, what happens when something breaks. |
| **[Scripts in Audiosurf Tweaker](scripting/tweaker.md)** | The Script Dev tab — create a project, open it in your editor, build, install, rebuild on save — and the Scripts tab, which manages what is installed. Where to start writing a script. |
| **[How Audiosurf is built](scripting/game-model.md)** | The important one. The game is a graph of *channels*, and everything else here only makes sense once that clicks. How to think about it, and how to find the piece you want. |
| **[Reading and writing the game](scripting/channels.md)** | Channel accessors, values, text, vectors, tables, and how to handle the fact that nothing is available immediately. |
| **[Reacting and intercepting](scripting/hooks.md)** | Running code when the game does something, and stopping the game from doing it. |
| **[Drawing](scripting/drawing.md)** | The HUD API: text, shapes, measurement, colours, and laying out against the overlay. |
| **[Menu tabs](scripting/menus.md)** | Your script's own tab in the overlay menu: switches, sliders, choices, colours, buttons, lists — and reading what the player set. |
| **[Saving data](scripting/storage.md)** | A folder of your own for settings and data: text and JSON, kept between sessions. |
| **[Worked examples](scripting/examples.md)** | Probes written to answer a question about the game, and one settings tab that survives a restart, annotated. Not shipped with the plugin — copy one in when you want it. |
| **[Projects: scripts made of many files](scripting/authoring.md)** | When one file stops being enough: `twllink --new`, modules and `require`, an editor that knows the API, checks at build time, installing, and errors that point at your own files. |
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

**Scripts are per-machine, not per-save.** Nothing a script *says* is uploaded: what reaches the online
leaderboards is the game's own score. A script that only reads the game and draws changes nothing there.
A script that changes the game's **logic** — scoring, the chain, timers, what matches — changes the score
the game itself then reports, and nothing in the Tweaker detects that or switches the game's online
services off. What that means in practice, and where the line is, is in
[Playing Audiosurf § Mods and the leaderboards](internals/playing-audiosurf.md#mods-and-the-leaderboards).

## Getting help

- The example scripts that ship with the Tweaker are meant to be read. `puzzlepro_hud.lua` in
  particular is heavily commented and demonstrates most of the API; `particles.lua` is the short one,
  and shows how a hook plus a single table write can take a piece of the game's own rendering
  behaviour away. `puzzlepro_hud.lua` is also a built script: it is made from a
  [project](scripting/authoring.md) of 17 modules, and each one is marked inside the file where it
  starts. Its design notes ride along at the top.
- If a script cannot find something, it says so in the notification feed. Read
  [Getting started § When something goes wrong](scripting/getting-started.md#when-something-goes-wrong).
- **[How Audiosurf works inside](internals.md)** is the map beyond this manual: the game from the
  player's seat with every term tied to its channels, the `.cgr` format, start-up and the life of
  groups, a long gameplay map with indices, and the native ABI.
- **To find something nobody has written up yet**, use the tools it was all found with —
  `DevTools\CgrPy` next to `TweakerUI.exe`, on your editor's `PATH` when the Tweaker opens it:
  `cgrpy query find "Undo"`, `cgrpy query writers StatCollector Points`. See the
  [CgrPy README](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Tools/CgrPy/README.md).
