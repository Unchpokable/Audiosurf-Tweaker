## Audiosurf Tweaker script project

This folder is a **Lua script for Audiosurf**, run inside the game by the Audiosurf Tweaker plugin. It is a
`twllink` project: modules in `src/`, entry point `src/main.lua`, manifest `twproject.json`. `twllink` links them
into one `build/<id>.lua` (plus `.lua.map`), and that one file is what the game loads.

**You cannot see the game.** Nothing you run shows you what the script did on screen. The user runs the game and
tells you; the plugin's log tells you the rest. Plan every change around that — skill `audiosurf-in-game-loop`.

### Start here

- **A vague, visual or jargon-heavy request — or the first request in this project — starts with skill
  `audiosurf-grill`.** "A beautiful HUD with fireworks", "make Eraser's undo push with overflow": find out what
  the user actually wants, check it can be done, write it down, and get a yes before writing code.
- Then the skill that fits:

| Skill | When |
|---|---|
| `audiosurf-grill` | first request, vague request, game jargon, anything that might change scoring |
| `tweaker-lua-api` | writing or fixing the script: the `tw.*` API, its lifecycle, its settings tab and files, `twllink` and its errors |
| `audiosurf-game-files` | where the game is, what may be read, what may be written (almost nothing) |
| `cgrpy` | finding the game's channel, handler or value that the docs do not name |
| `audiosurf-in-game-loop` | building, installing, and getting the user to check it in the game |
| `audiosurf-binaries` | questions only the game's native code answers (only if this skill is installed) |

### Commands

| | |
|---|---|
| `twllink --json` | build; read `diagnostics` (each has `code`, `file`, `line`, `message`); exit 0 built, 1 errors |
| `twllink --install` | build and copy into the game; needs `AUDIOSURF_DIR` or `--game <Audiosurf folder>` |
| `cgrpy query find "<regex>"` | search the game's channel graph; `cgrpy` alone lists the tools |

When the Tweaker opened this editor, `twllink` and `cgrpy` are on `PATH` and `AUDIOSURF_DIR` is set. Otherwise
read `.tweaker/local.json`: `devtools` holds `Binaries\twllink.exe` and `CgrPy\cgrpy.cmd`, `game` is the
Audiosurf folder. No file, no value — ask the user.

### Documentation

`.tweaker/docs/` — the manuals shipped with this Tweaker. `.tweaker/docs/scripting.md` (the scripting manual)
and `.tweaker/docs/internals.md` (how the game works inside) are the indexes; the API, name by name, is
`.tweaker/docs/scripting/api-reference.md`. Read them before guessing; they are the version that matches the
user's Tweaker.

### Rules

- **The game is not ours, and it is the user's copy.** Read its files; write nothing into it except the built
  script, through `twllink --install`. The script keeps its own files in `engine\TweakerStuff\ScriptData\<id>\` —
  read them, do not write them. Never commit, upload or hand around anything from the game folder.
- **Install nothing yourself.** If a tool is missing (`uv`, a decompiler), say what it is, where to get it and
  the command — the user runs it.
- **Nothing is verified until it worked in the game.** A channel name, a doc, a finding: each is a lead until a
  probe script showed it in the running game.
- **Changing the game's logic changes the score it sends to the leaderboards.** Making the game harder is fine.
  For anything that plainly makes scoring easier, say that it is cheating, ask whether that is really the goal,
  and carry on if it is.
- **Answer in the user's language.** Do not use words like "channel" or "vtable" with someone who has not used
  them first.
