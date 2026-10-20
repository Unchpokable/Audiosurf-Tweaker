---
name: tweaker-lua-api
description: Writing, fixing or reviewing the Lua script in this project - the tw.* API of the Audiosurf Tweaker plugin, the script lifecycle (on_tick, on_frame, on_ready), reading game channels, hooks and mutes, drawing on the HUD, the script's own settings tab in the overlay menu (tw.ui), its own files (tw.storage), twllink builds and TWxxxx diagnostics. Use for any change to src/ or twproject.json.
---

# Writing the script: the `tw` API and `twllink`

The script runs **inside the game process, on the game's own thread**, through the Tweaker plugin's LuaJIT. Slow
code is a frame drop; an error loses one call and is counted. Everything the script can touch is in the global
table `tw` — there is no other way into the game.

The manuals are in `.tweaker/docs/` and match the user's Tweaker. This skill is the short version; for the
exact signature of anything, open the reference — never guess a name.

| Question | Read |
|---|---|
| a function's exact name, arguments and return | `.tweaker/docs/scripting/api-reference.md` |
| how the game is built, where to look for something | `.tweaker/docs/scripting/game-model.md` |
| reading and writing channels, tables, vectors | `.tweaker/docs/scripting/channels.md` |
| hooks, intercepting, muting | `.tweaker/docs/scripting/hooks.md` |
| drawing, layout, sharing the screen | `.tweaker/docs/scripting/drawing.md` |
| a tab of the script's own in the overlay menu: switches, sliders, choices, buttons | `.tweaker/docs/scripting/menus.md` |
| the script's own files: settings, records, caches, JSON | `.tweaker/docs/scripting/storage.md` |
| what is forbidden or absent, budgets, writes | `.tweaker/docs/scripting/limits.md` |
| the project, `require`, every `TWxxxx` code | `.tweaker/docs/scripting/authoring.md` |
| what a game term means, where it lives | `.tweaker/docs/internals/playing-audiosurf.md` |
| the gameplay groups with indices and traps | `.tweaker/docs/internals/gameplay-map.md` |

Working code for the common shapes — read a value and draw it, count an event, replace a piece of the game's HUD,
place a widget, survive a group reload, write and put back, a settings tab that survives a restart — is in
`references/patterns.md` next to this file.
Start from one of those rather than from a blank file.

## The rules that shape every script

**Declare at the top level, act in callbacks.** The top level of a script (and of every module) runs while the
script loads — for the plugin loaded with the game, before the game has loaded a single group. So the top level
creates handles, registers callbacks, declares its menu tab, sets constants, and **reads nothing** from the game
(its own settings file is the exception — read it there, see below). Registering (`tw.on_*`, `tw.mute`, `tw.ui.*`
and `on_change`/`on_click`) works only there and inside `tw.on_ready`; from any other callback it is refused.

**Nothing runs until the game has loaded.** `on_tick`, `on_post_tick` and `on_frame` are held back until the
plugin's state is `ready`; `tw.on_ready(fn)` runs once, when that happens (also for a script switched on
mid-session). `tw.on_state` and `tw.on_group` run in every state. No "has it loaded" guard of your own is needed.

**Two frames.** `tw.on_tick` / `tw.on_post_tick` run on the engine frame, before / after the game evaluates its
graph — read and compute there. `tw.on_frame` runs on the overlay's drawn frame — **draw there and only there**;
`tw.hud.*` outside it silently does nothing. Compute in `on_tick`, draw in `on_frame`. `tw.frame` is the engine
frame number; `tw.dt()` the seconds since the previous frame.

**Handles, once.** `tw.float_ch`, `tw.string_ch`, `tw.vector_ch`, `tw.matrix_ch(group, name)` and `tw.array`,
`tw.array_vec(group, column, cursor)` are cheap and never fail; they resolve on first use and keep the result.
Created inside a callback, they search thousands of channels every frame. `:get()` returns `nil` whenever the value
is not available — the group is not loaded (a run's groups do not exist in the menu), or the name is wrong.
**Every `:get()` result is checked for `nil`.** `:dead_end()` says the group is loaded and the name is not in it.

**A group** is a pool name (`"Renderer"`) or a file name without `.cgr` (`"StatCollector"`); `tw.groups()` lists what
is loaded. **A channel** is a name or an index. Names are not unique inside a group and a name lookup takes the
first — when `cgrpy query` says a name is ambiguous, use the index and comment why. Indices are exact but belong to
this build of the game.

**Hooks get no arguments.** `tw.on_call(group, name, "after" | "before", fn)` runs `fn()` when the game calls that
channel. To know what the call was about, read channels the handler itself writes, from an `"after"` hook. Do
**not** read the handler's group parameters: a group parameter holds whatever it held last, not what is being
passed, and looks plausible while being wrong. Returning `false` from a `"before"` hook cancels the game's handler
for that call. Keep hook bodies short: accumulate, and draw later in `on_frame`.

**Muting takes behaviour away.** `tw.mute(group, name)` makes the game's calls to a channel do nothing; `:on()`,
`:off()`, `:set(bool)` toggle it. A mute belongs to the script: while the script is suspended or off, the game gets
the channel back. Mute only what nothing else reads — muting a channel whose result the game uses leaves the game
computing quietly wrong answers. `cgrpy query parents` before muting.

**Writes are the dangerous part.** `handle:set(...)` lands immediately and is refused while the game loads. A
number outside what the game expects is your bug, not the API's. A vector write also writes the numeric channels
wired to its components. `array:set` refuses a row that does not exist. Before writing anything, trace what feeds
the channel and what reads it (skill `cgrpy`); an untraced write is a guess. Put back what you changed in
`tw.on_unload`.

**Scripts share the screen — lay out by what is taken.** The overlay's own widgets (notification feed, pins,
watermark, menu) and other scripts are drawn too, and each is a rectangle you can ask for.
`tw.hud.place(w, h, corner, margin)` finds a free spot near a corner and takes it; for a widget with a place of its
own (top centre, where the game's score was), position it from the viewport and check it against
`tw.hud.occupied()`; `tw.hud.claim` holds a box on frames you do not draw. Never assume a corner is free.
**Do not lay out from `tw.hud.safe()`**: it is a full-width band as tall as the notification feed, from before
scripts shared the screen, and a widget measured from it sits far down over the track — the first thing both
PuzzlePRO HUD and the first agent-built HUD got wrong. Patterns 1, 4 and 5 in `references/patterns.md`.
Colours are packed `0xAABBGGRR`; `tw.theme(name)` follows the overlay's palette, `tw.rgb`, `tw.alpha`, `tw.fade`
build and adjust.

**Scripts run in the order the user set** in the Scripts tab. A script cannot read or change it. Two parts that
must run in a fixed order belong in one script.

## Settings: a tab in the overlay menu, and files that survive

A script can have **its own tab in the overlay menu** (`tw.ui`, API 3) and **its own folder of files**
(`tw.storage`). Together they are how a script gets settings the player changes in the game and keeps between
sessions — the usual shape is pattern 10 in `references/patterns.md`.

**The tab** (`.tweaker/docs/scripting/menus.md`):

- `tw.ui.tab{ label = "..." }`, then controls on it or on its `group(...)`s (two levels deep at most): `toggle`,
  `slider`, `number`, `choice`, `color`, `button`, `list`, `text`, `separator`. Each takes a table — `label`,
  `tooltip`, `enabled`, `visible` and the fields of its kind; a bare string is the label.
- **Declared where callbacks are registered**: the top level or `tw.on_ready`. Anywhere else you get an inert
  handle and a warning in the Scripts tab.
- `ctl:get()` works anywhere, any time, before the game has loaded too — cheap enough for every frame. A `choice`
  answers `{ text, index }` (the same table every call), a `list` a row number from 1 or `nil`, a `color` a packed
  colour like `tw.rgb`.
- `ctl:on_change(fn(value, committed))`, `button:on_click(fn)`: only **the player's** changes; `committed` is the end
  of a gesture (a slider or colour let go). Save there. `ctl:set(v)` is the script writing: it never calls
  `on_change`, and is ignored while the player is dragging that control.
- The plugin draws the tab, not your Lua — it costs the script nothing. It sits after the built-in tabs, in the
  run order; Reload keeps its place.
- **Labels are Latin only**: the overlay's fonts have no Cyrillic, so Russian labels draw as `?`. Write the tab in
  English even when you talk to the user in Russian, and tell them why. Labels up to 96 bytes, 16 options, 512 list
  rows, 1024 controls.
- **Not there:** typing text, catching a key, a picture or preview inside the tab, tabs in a tab, a slider step.
  No "reset to defaults" built in — a button and `:set()` calls.

**The files** (`.tweaker/docs/scripting/storage.md`):

- One folder per script: `engine\TweakerStuff\ScriptData\<file name without .lua>\` — for a project, the `id`.
  **A new `id` is a new, empty folder**: one more reason never to change it.
- `tw.storage.read_text` / `write_text` / `read_json` / `write_json` / `exists` / `remove` / `list`. Names are flat:
  `[A-Za-z0-9_.-]`, up to 64, not starting with a dot. 1 MiB a file, 64 files, 8 MiB in all.
- **Failure is a result, not an error**: `nil, reason`. `tw.storage.read_json("settings.json") or {}` is the whole
  first run. Only a wrong argument raises.
- **Read at the top level** — the first read of a file is a real disk read; after that it is memory. Writes return at
  once and reach the disk within a tenth of a second (whole-file replace, crash-safe), so writing on every change is
  fine. Save **on the change** (`committed`), not at exit: `on_unload` does not run when the game quits.
- JSON: object ↔ table with string keys, array ↔ `1..n`, `null` ↔ missing. Refused: NaN and infinities, mixed or
  holed keys, functions, cycles. Save a `choice` by its **text**, not its index.
- Nothing else on the disk is reachable — not another script's folder, not the game's files. A file in the folder
  cannot be drawn as an image either.

## Messages, and what reaches the user

| Call | Where it shows |
|---|---|
| `tw.notify(msg)` | a toast in the game — for the player; capped at a handful a minute per script |
| `tw.warn(msg)` | the script's row in the overlay's Scripts tab, and the script log |
| `tw.error(msg)` | the same, plus one toast per message once the game has loaded; does not stop anything |
| `tw.pending(msg)` | marks the script as waiting while it keeps saying it — say it every frame you wait |
| `tw.log(msg)`, `print` | the plugin's debug log only — **silent in a normal install**; do not rely on them |
| `error(...)` | Lua's own: stops that call, is reported with a traceback |

Every message is **deduplicated**: the same text from the same place is one record with a counter, and numbers in
the text do not make it new — `"x = 1"` and `"x = 2"` are one record. To watch a value change, draw it on the HUD
and ask the user what it shows, or have a probe record it into a file of the script's own and read that file
yourself (skill `audiosurf-in-game-loop`, "Probes"); the log holds the first occurrence (and the 10th, 100th, …). The log file is
`engine\TweakerStuff\Logs\scripts-<date>_<time>.log` in the game — skill `audiosurf-in-game-loop`.

## Limits that will not move

- **Removed from the environment:** `ffi`, `io`, `debug`, `jit`, `package`, `require` (the global — a project's
  `require` is `twllink`'s), `dofile`, `loadfile`, `load`, `loadstring`, `os.execute`, `os.remove`, `os.rename`,
  `os.tmpname`, `os.exit`, `os.getenv`, `os.setlocale`, `newproxy`, `collectgarbage`. `string`, `table`, `math`,
  coroutines, `os.clock`, `os.time`, `os.date` work.
- **Absent by design:** reading the keyboard or the mouse directly (input comes only through the script's own tab),
  typing text, hotkeys, files outside the script's own folder, script-to-script communication, the Tweaker's own
  state (skins, playlists, tweaks), images of your own (`tw.hud.icon` draws the plugin's built-in icons only — not a
  file from the script's folder), shapes other than rectangles and lines, sound. A request that needs one of these
  cannot be done by a script — say so (skill `audiosurf-grill` has the full list of verdicts). Settings, buttons and
  saving between sessions **can** be done: the section above.
- **Failure budget:** 5 errors within 300 engine frames, or handlers averaging over 4 ms a frame for 120 frames,
  **suspend** the script (its mutes lift, its handlers stop) until the user presses Resume or Reload. Over 0.5 ms a
  frame is shown to the user as a cost.
- **API version 3** (`tw.api_version`). `tw.ui` and `tw.storage` are new in 3; `tw.hud.place`/`occupied`/`claim` in 2.
  A name newer than the project's `api` in `twproject.json` is warning TW0303 — raise `api` to 3 (new projects
  already have it). A script whose `api` is above the user's Tweaker does not run; the Scripts tab says which it
  needs.

## The project and `twllink`

```
twproject.json   id, name, version (MAJOR.MINOR.PATCH), description, api
src/main.lua     the entry point; every other file in src/ is a module: src/draw/rail.lua is require("draw.rail")
build/           <id>.lua and <id>.lua.map - generated, never edited
.twllink/        tw.def.lua and the manifest schema for the editor - generated
```

- `require` takes **one string literal**, with dots: `require("draw.rail")`. Case must match the file. No cycles.
  `main` cannot be required. Files nothing requires are not in the script (warning TW0109).
- Top-level `local`s are per module; a global assigned in one module is visible in all (warning TW0305 — use
  `local`). Never write into `tw` (error TW0304): every script shares it.
- **The `id` is the script's identity in the game** (its file name). Do not change it once the script is installed
  or shared: a new id is a new script, and the old one stays installed.
- Build with **`twllink --json`** and read `diagnostics`: `severity`, `code`, `file` (relative to the project),
  `line`, `column`, `message`. Exit code 0 built, 1 errors, 2 bad command line, 3 the tool failed. A failed build
  writes and installs nothing. Every code is explained in `.tweaker/docs/scripting/authoring.md#diagnostics`.
- TW0302 (a `tw.*` name that does not exist) comes with the closest real name — check it against the reference
  rather than accepting it blindly. TW0301 is the sandbox: there is no way around it.
- Positions in the game's messages are already mapped back to `src/` files when the `.lua.map` is installed with
  the script, which `twllink --install` does.

## Traps that look like working code

- **Reading too early.** A value read before the game computed it is what the game's author left in the file —
  plausible and wrong. Read in callbacks, after `ready`.
- **Group parameters** read inside a hook (see above).
- **A table read mid-run that is only right at the end** (`StatCollector`'s `Stats: TrafficColorCounts` is counted
  before the whites exist and corrected only by the final scoring). The gameplay map lists these.
- **The first match of a name** being the wrong channel.
- **A by-name handle created in `on_frame`**: correct, and a lookup over the whole group every frame.
- **Muting a reader**: the game keeps running on a stale value, nothing fails.
- **Assuming the game's HUD or the overlay is not there**: use `tw.hud.place`.

## The game's own traps — read before counting, scoring or replacing the HUD

PuzzlePRO HUD, the first script that read most of the run and replaced half the HUD, walked into each of these, and
each looked right until a player noticed. The details are in `.tweaker/docs/internals/gameplay-map.md`; its design
notes, as the story of each mistake, open the shipped `puzzlepro_hud.lua` in the game's `engine\TweakerStuff\Scripts\`
(read it, never edit it). Anything that counts blocks, shows the score or takes over a piece of the HUD: go through
this list first.

- **Blocks per colour: count track rows, not `Do_CollectCar` calls.** Power-up blocks go to `Do_CollectQuestionBox`
  instead, and leave a ghost copy of another row behind; counting events is wrong per colour and passes 100 %.
  `references/patterns.md`, pattern 3.
- **The blocks a track has: walk `Stats: TrafficPattern` after `Do_ReseteWhiteWildBlocks`.** `Stats:
  TrafficColorCounts` is counted before whites and wilds are placed over coloured rows, and the block count never
  changes, so "rescan when `TotalCarCount` changes" never fires.
- **A restart from pause fires `Do_ResetSimpleStats`, not `Do_ResetStats`.** Reset on both.
- **`Do_RenderMoneyParticles` (#3980) pays the score.** It looks like a render branch; muting it stops all scoring.
  Hide the coins with `MoneyFloater_GiveAlpha` instead. Every other HUD cut — and what not to cut — is in the map's
  HUD table.
- **The record and the medals are final scores.** `PB` and the end-of-run medals use `PointsWithGridBonus` = `Points ×
  (1 + Σ bonuses)`, and Clean Finish counts while the board is empty right now. Raw `Points` next to the record
  reads as a bad run.
- **Read the game's numbers, never retype them.** `MatchCollectionTicks` differs per character, league and song;
  `GoldRequirement` is per run; `LeagueID` belongs to the character; the chain's worth is `Envelope` #643 — read it as
  a channel.
- **The chain countdown freezes while blocks slide.** Read `MatchCollectionTicks - MatchTimer` every frame; a timer of
  your own, or a smoothed one, shows time the player does not have.
- **One match is one particle per block**, each landing on its own; the score grows when they land, not when the
  match is made. Merge per colour; a merged entry ends with its last particle.
- **Several numbers that depend on one value** (the chain's colour, bar and multiplier) come from one tweened value,
  not three tweens — or they arrive at different times.
- **Position from what is taken** (`place`, `occupied`), never from `tw.hud.safe()`; text over the sky needs a
  backdrop (`.tweaker/docs/scripting/drawing.md`, "Coordinates").
- **A bar for the chain grows with its bonus** (`Envelope` #643, 0–3), not with `ChainCount` (0–100): the game's own
  bar does, and a bar linear in `ChainCount` sits nearly empty through a whole good run.
- **Song progress as the game shows it** is time: its bar (`XX_gui::Timer2`) is `VisMusic::SongPlayRemaining /
  SongLength`, shrinking. `Player::CurrentPlayerRing / Highway::NumRings` is distance along the track — close, but
  another number; say which one you show.

## Before you hand it over

1. `twllink --json` succeeds with no warnings you cannot explain.
2. Every channel `:get()` is `nil`-checked; nothing of the game is read at the top level; nothing is registered or
   declared outside the top level or `on_ready`.
3. Drawing only in `on_frame`; hooks only accumulate.
4. Every write and mute traced (skill `cgrpy`), and every write put back in `on_unload`.
5. If it has settings: declared at the top level, read from the settings file at the top level, saved on
   `committed`, Latin labels, a choice saved by its text, and a first run with no file works.
6. Every index has a comment saying why the name would not do.
7. Then the user checks it in the game — skill `audiosurf-in-game-loop`. Until then, it is untested.
