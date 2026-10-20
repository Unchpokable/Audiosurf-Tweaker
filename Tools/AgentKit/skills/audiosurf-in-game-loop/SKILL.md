---
name: audiosurf-in-game-loop
description: Getting a change checked in the running game - build, install, have the script reload, tell the user exactly what to do and what to look for, then read the plugin's script log (engine\TweakerStuff\Logs\scripts-*.log) and the files the script itself wrote (engine\TweakerStuff\ScriptData\<id>\), and iterate. Also probe scripts that record values over a run, the user's "go wild" permission and what to do when the game breaks. Use after every change that should be seen in the game, and whenever the user reports what happened there.
---

# Checking it in the game

**You cannot see the game.** Nothing you can run shows what the script draws, whether a hook fires at the moment
you think, or whether the game still plays right. The user runs the game and is your eyes; the plugin's script
log tells you what the scripting layer saw. Say this plainly when it matters, and never report something as
working because it built.

## The loop

1. **Say what the check is** before building: what the user should do in the game, and what they should see if
   it works — "start any song as Eraser; a white counter appears top right and goes up by one per block".
2. **Build**: `twllink --json`. Fix every error; explain or fix every warning.
3. **Install**: `twllink --install`. It needs the game: `AUDIOSURF_DIR` (set when the Tweaker opened this editor),
   or `--game "<game>"` with `game` from `.tweaker/local.json`. Only this script's `.lua` and `.lua.map` are
   copied; a failed build installs nothing.
4. **Get the new build running** — which of these applies is something to ask once and remember:
   - the Tweaker's **Script Dev** tab has *Rebuild on save* with *and install into the game*, and the game is
     running and connected: every saved file rebuilds, installs and reloads by itself. Then you do not run step 3;
     but note that a half-finished edit you save goes into the game too;
   - the game is running: the user presses **Reload** on this script's row in the overlay's Scripts tab (in the
     game, **Insert** opens the overlay), or **Rescan** there if the script is new; *Build and install* in
     Script Dev does the same for them;
   - the game is not running: it loads the script when it starts. A new script is switched on unless the user
     switched it off before.
5. **Hand over** — a short message the user can act on without knowing anything about scripts:
   what to start (mode, character, any song or a specific kind), where to look, what "working" looks like, and
   what to send back: what they saw, a screenshot if anything is drawn, and whether the game behaved normally.
   If the script has a tab of its own: **Insert** opens the overlay, and the tab is in the menu after the built-in
   ones (Skins … Scripts), under the label you gave it — say which controls to try.
6. **Read the log** (below) — after the user has done it, or as soon as they report anything odd.
7. **Decide**: fixed, next step, or a new probe. One change per round where you can; a round with five changes
   that fails tells you nothing about which.

## The script log

`<game>\engine\TweakerStuff\Logs\` holds one file per game session, `scripts-YYYY-MM-DD_hh-mm-ss.log`. **The newest
is the last by name**; the one before it is the previous session — the one to read after a crash, since the user
has probably started the game again since. The game writes it while running, at most a tenth of a second behind.

```
===== 2026-10-18 20:41:43 TweakerPlugin 0.1.0.0, LuaJIT 2.1.1787165859 =====
time         frame    kind      script: where: text
20:41:43.538 f0       script    (layer): 5 script(s) in Scripts, run order: as-is-particles.lua, my_hud.lua, ...
20:41:43.540 f0       script    my_hud.lua: loaded
20:41:48.562 f1338    engine    (layer): ready (start group XX_StartHere.cgr, 109 groups loaded)
20:41:54.025 f2554    warning   my_hud.lua: main.lua:2: something said with tw.warn
20:41:55.685 f2853    error     my_hud.lua: on_frame: draw/rail.lua:6: attempt to index a nil value
    stack traceback:
    	...
20:41:57.481 f3176    script    my_hud.lua: switched off
```

What to look for, for **this** script's file (`<id>.lua`):

| Line | Means |
|---|---|
| `script … loaded` / `reloaded` / `switched on` | the build you installed is running |
| `script … loaded: failed, not running` | it threw while loading — the `error … load:` line above says why |
| `error … on_frame:` / `on_tick:` / `on_call:` … | a callback threw; positions are files in `src/` (the map was installed with it) |
| `repeated … - N times since fK` | the same message again, still happening |
| `suspended …` | the plugin stopped it (5 errors in 300 frames, or over 4 ms a frame); nothing of it runs until Resume or Reload |
| `info … map: … is ignored` | the installed `.lua` was edited by hand — rebuild and install both files |
| `engine … busy`, then `ready` | a run (or other content) loading — a run starting |
| `dropped … line(s) lost`, `limit` | something flooded the log; its own messages are what to fix |

A script that is not in the log at all was not found (wrong folder, not installed) or is switched off. Every line
has the engine frame (`f2853`): an error with a frame number shortly after `engine … ready` happened as the game
came up, one after a `busy`/`ready` pair happened in a run.

**The log does not show values over time.** Messages are deduplicated, and numbers in them do not make a new
message: `tw.warn("x = " .. x)` every frame logs the first and then only counts. To see a value change, record it
into a file (below) or draw it on screen and ask the user; to know *that* something happened, `tw.warn` once from
the place it happens. `print` and `tw.log` do not reach this file at all.

## The script's own files

A script keeps its files in `<game>\engine\TweakerStuff\ScriptData\<id>\` (`tw.storage`): its settings, and
whatever a probe records. **You may read them** — that is how you see what the script saved, and what a probe
measured during the user's run. Do not write or delete them: while the game runs the plugin holds them in memory
and writes them back, and the settings in there are the user's. A settings file that is wrong is fixed by the
script (it must survive a bad or old file) or by the user deleting it — say so, with the path. Removing the script
does not remove its folder.

## Probes

A probe is temporary code whose only job is to answer one question in the game. Keep it in its own module
(`src/probe.lua`, required from `main.lua`), so it comes out by deleting one `require` line — and take it out
before calling the work done.

- **Does this handler fire when I think?** `tw.on_call(...)` + `tw.notify("…fired")` (capped at a handful a
  minute — for frequent events, count and draw the count).
- **What value does this channel have during a run, and how does it change?** Record it: the probe writes a line
  every few frames into a file of its own, and you read the file after the run (pattern 11). A **Mark** button
  on the probe's tab lets the user note the moment they mean ("press it when the chain drops") — the mark lands in
  the file between the values. Faster than any amount of "what did it show?", and exact.
- **What does it look like at one moment?** Draw it with `tw.hud.text` in a spot from `tw.hud.place`, and ask for a
  screenshot.
- **A tuning question** — which size, colour or timing is right: put the candidates on a slider or a choice in the
  probe's tab and let the user try them during a run; then ask which one, or record the value they settled on.
- **Is this group loaded when I need it?** `tw.on_group(name, fn)` with a `tw.warn`, or draw `tw.state()` and
  `handle:valid()`.

Patterns for each are in skill `tweaker-lua-api` (`references/patterns.md`).

## "Go wild"

The user may tell you to stop asking and iterate on your own — build, install, read the log, change, repeat.
When they do, say once, plainly: you still cannot see the game, so a change can break how it plays or make it
crash, and a game broken that way is theirs to put right (the steps below). Then go ahead. The permission covers
how often you build and install; it never covers writing anywhere but this script's own files in `Scripts\`
(skill `audiosurf-game-files`), and never covers installing software.

## When the game breaks

Tell the user, in this order of preference:

1. **Switch the script off** — the toggle on its row in the overlay's Scripts tab, or in the Tweaker's Scripts tab.
   Its hooks and mutes go away at once.
2. **The game will not start or crashes at once** — remove this script from the game: delete
   `engine\TweakerStuff\Scripts\<id>.lua` and `<id>.lua.map`. You may do that yourself if they ask; these are the
   files you installed.
3. **It still breaks** — the plugin can be switched off as a whole: an empty file named `DISABLE` in
   `engine\TweakerStuff\`. That is the user's switch: tell them about it, do not create it.
4. Steam's *Verify integrity of game files* puts the game's own files back — needed only if something other
   than a script changed them, which nothing in this project does.

Then read the previous session's log for the last thing that happened.
