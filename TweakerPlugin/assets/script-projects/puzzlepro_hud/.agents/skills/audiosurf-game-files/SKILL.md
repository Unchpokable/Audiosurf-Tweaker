---
name: audiosurf-game-files
description: Finding the user's Audiosurf install and knowing what in it may be read, what may be written (only the built script, through twllink --install) and what must never be touched or shared - .cgr files, engine DLLs, the plugin, scripts.cfg, the scripts' own ScriptData folders, the DISABLE switch. Use before reading or writing anything outside this project folder, and whenever a path to the game is needed.
---

# The game's files: where they are, and hands off

Audiosurf is someone else's program, installed on the user's computer. This project reads it and puts **one
file pair** into it — the built script — and nothing else. Everything below follows from that.

## Finding the game

In this order; stop at the first that works:

1. **`AUDIOSURF_DIR`** in the environment — set when the Tweaker opened this editor. It is the Audiosurf folder
   or its `engine` folder.
2. **`.tweaker/local.json`** in this project — `game` is the Audiosurf folder, as the user chose it in the
   Tweaker; `devtools` is the Tweaker's `DevTools` folder. The Tweaker rewrites this file whenever it opens the
   project; it is not committed.
3. **Ask the user.** Do not search their disks for it. (`cgrpy` has its own search, Steam included, for its own
   use — that does not make the path safe to write to.)

A folder is the Audiosurf folder when it has `engine\QuestViewer.exe`. The Tweaker plugin is installed in it
when `engine\TweakerStuff\` exists; without that, nothing here works and only the Tweaker can fix it — tell the
user to install the plugin from the Tweaker.

## The layout

```
Audiosurf\
  Audiosurf.exe                  the launcher
  engine\
    QuestViewer.exe              the game itself (Quest3D's player)
    HighPoly.dll                 the engine core
    channels\                    one DLL per channel type, named by GUID; TweakerPlugin.dll lives here too
    channels.lst                 channel type GUID -> name
    XX_StartHere.cgr, ...        channel groups: the game's logic, data and assets
    Actors\ Environment\ gui\ Render\ Scores\ Stage\ Intros\ SongSelector\ Effects\ ...   more groups
    textures\ sounds\ ...        assets
    Cache\                       Audiosurf Radio songs the game downloaded - different for every player
    TweakerStuff\                Audiosurf Tweaker's, everything below
      Scripts\                   *.lua (+ .lua.map) - the installed scripts
      Config\scripts.cfg         which scripts are off, and their run order
      Config\overlay.cfg         the overlay's settings
      ScriptData\<script>\       each script's own files (tw.storage): settings, records, what a probe wrote
      SkyboxReplacer\            custom skies
      Logs\TweakerPlugin.log     the plugin's start-up log
      Logs\scripts-<date>_<time>.log   one per game session: everything the scripts said (newest = last by name)
      DISABLE                    if present, the plugin stays inert - the user's emergency switch
```

## What you may do

| | |
|---|---|
| **Read anything** | `.cgr` through `cgrpy` (it parses the container); logs, configs, `channels.lst` as text |
| **Write** | the built script and its map into `engine\TweakerStuff\Scripts\` — **only** through `twllink --install`, which copies exactly `build/<id>.lua` and `build/<id>.lua.map` |
| **Read the script log** | `engine\TweakerStuff\Logs\scripts-*.log`, the newest by name — skill `audiosurf-in-game-loop` |
| **Read this script's files** | `engine\TweakerStuff\ScriptData\<id>\` — what the script saved, what a probe recorded |

The script itself writes into its `ScriptData\<id>\` folder through `tw.storage`; that is the script's doing, inside
the game, and the only place it can write. You do not write there: the plugin holds those files in memory while the
game runs, and the settings in them are the user's.

## What you must not do

- **Edit, patch, replace or delete** anything in `engine\` outside `TweakerStuff\Scripts\<this script's id>.*`:
  `.cgr` files, DLLs (the game's or the plugin's), `channels.lst`, assets, `QuestViewer.exe`. A broken `.cgr` is a
  broken game, and Steam's "verify files" may or may not put it back.
- **Touch other scripts** in `Scripts\` — not even to "clean up". They are the user's or the Tweaker's. The same
  for `ScriptData\`: other scripts' folders hold their settings and the user's data — leave them alone — and this
  script's folder is read-only for you.
- **Edit `scripts.cfg`.** While the game runs it belongs to the plugin and is rewritten by it; switching scripts
  and their order is the user's, in the Scripts tab of the overlay or the Tweaker.
- **Create or delete `DISABLE`.** It is the user's switch for "the plugin broke my game". You may *tell* the user
  it exists.
- **Commit, upload, paste or attach** anything from the game folder — files, `.cgr` dumps, extracted textures,
  models, sounds, `cgrpy`'s cache (`proj.pkl`). Quoting a channel name or index in a comment is fine; shipping the
  game's content is not. This project's `.gitignore` must never be loosened to let any of it in.
- **Install** the plugin, the Tweaker, Python, `uv`, a decompiler or anything else. Say what is needed and how
  to get it; the user does it.

## The user's own copy

Reverse-engineering work — decompiler databases, experimental patches, the `cgrpy` mesh tools — belongs on a
**copy of the game the user made for it**, never on the Steam install they play from. If a task needs that and
the user has only the live install, say so and stop; do not improvise on the live one. `cgrpy meshes replace`
refuses a path that looks like Steam for this reason. Even on a copy, results stay on the user's machine.

## When a path does not work

- `twllink --install` fails with TW0203: no game given, not an Audiosurf folder, or no `engine\TweakerStuff\`
  (plugin not installed). The build itself succeeded; fix the path or ask.
- A path from `local.json` that no longer exists: the user moved the game — ask, and tell them the Tweaker will
  update the file when they set the game folder there.
