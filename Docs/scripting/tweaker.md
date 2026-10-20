# Scripts in Audiosurf Tweaker

[← back to the index](../scripting.md)

Everything in this manual works with nothing but a text editor and the game: a script is a file you
drop in a folder. Audiosurf Tweaker adds two tabs that make the round trip shorter.

- **Script Dev** is where you write scripts. Create a project, open it in your editor, build it,
  install it into the game — and with the game running, see it reload there the moment you save.
  **If you are starting a new script, start here.**
- **Scripts** is where you manage what is installed: switch scripts on and off, see which came with
  the Tweaker and which are yours, and put back a bundled script you edited or deleted.

Both are in the Tweaker's menu, below Quick Player.

## Script Dev: writing a script

### Your first project

1. Open **Script Dev** and press **+** (New project).
2. Type a **Name** — anything, on one line: `My Speed HUD`. The **Script file (id)** below fills itself in
   from it: `my_speed_hud`. That is the script's file name in the game, `my_speed_hud.lua`, and
   [the script's identity](authoring.md#twprojectjson) — lower-case letters, digits and `_`, so there are
   never two spellings of one name. You can change it, but do it now rather than after people have your
   script.
3. Optionally, a **Description**: what the script does, and anything a player should know before
   switching it on. It wraps while you type, but it is saved as one line — that is what the script's
   header can hold.
4. Pick what to **Start from**:

   | | |
   |---|---|
   | **HUD** | Draws over the game with `tw.hud` — the most common kind of script. |
   | **Hook** | Counts something the game does, through `tw.on_call`, and draws the count. |
   | **Empty** | Just the project, with an empty `main.lua`. |

5. **Create.**

The project is made in the Tweaker's own folder, `DevTools\Sources\my_speed_hud\`, next to
`TweakerUI.exe`. It is an ordinary [project](authoring.md) — the same thing `twllink --new` makes — and
it builds as it is.

To work on a project that lives somewhere else, press the folder button (**Open a folder as a
project**) and pick a folder with a `twproject.json` in it. It stays where it is; the Tweaker only
remembers where.

### Open it in your editor

The **Editor** card lists the editors the Tweaker found on your computer: Visual Studio Code, Cursor,
Sublime Text, Notepad++, Vim and Neovim. Pick one — the choice is remembered — and press
**Open in …** on the project.

The editor opens the project folder with two things set up for it:

- **`twllink` is on its `PATH`**, so `twllink` works in the editor's own terminal without you
  installing anything or changing your system settings;
- **`AUDIOSURF_DIR` points at your game**, so `twllink --install` in that terminal knows where to
  install;
- **`cgrpy` is on its `PATH` too** — the tools the game was reverse-engineered with, for finding the
  channel you need when the manual does not name it: `cgrpy query find "Undo"`. They need
  [uv](https://docs.astral.sh/uv/) installed; see the
  [CgrPy README](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Tools/CgrPy/README.md), which
  also ships as `DevTools\CgrPy\README.md` next to `TweakerUI.exe`.

In VS Code and Cursor, install the **Lua** extension (by sumneko) and every `tw.*` name completes as
you type — see [Your editor](authoring.md#your-editor). Notepad++ has no language server, so it gets
none of that.

**An editor that is already running** keeps the environment it was started with: VS Code and Cursor
hand a new folder to the window that is already open, and its terminal does not see `twllink`. When
that is the case the Tweaker asks first. Close the editor and open the project from here, or open it
anyway and build from the Tweaker instead of the terminal.

### Build and install

- **Build** writes `build/<id>.lua` and its `.lua.map` inside the project.
- **Build and install** also copies both into the game's `engine\TweakerStuff\Scripts\`. If the game
  is running and the Tweaker is connected to it, the game **reloads the script straight away** — the
  status line says *Built, installed and reloaded*. Otherwise the game picks it up the next time it
  starts, or when you press Rescan (new script) or Reload (updated one) in the overlay's Scripts tab.

A build with errors installs nothing: whatever was in the game stays there and keeps running.

Under the status line is every error and warning, each with its place — `src/draw/rail.lua:41:7` —
its [code](authoring.md#diagnostics) and its message. **Double-click one** and your editor opens on
that line.

If your project's id is the same as a script that comes with the Tweaker, installing asks first: yours
would replace the bundled one, and the next Tweaker update would move yours aside as a `.old` copy and
put the bundled one back.

### Rebuild on save

Switch on **Rebuild on save**, and every time you save a file of the project, it builds by itself —
and, with **and install into the game** on as well, installs. With the game running and connected,
that means: save in the editor, look at the game, see the change. It keeps working when you switch to
another tab of the Tweaker; it watches the project you have selected.

A script that builds but throws when it loads shows up as *failed* in the game, with its error. It
stays switched on, so the next save that fixes it simply runs.

### Editing and removing a project

The pencil button edits the name, id, version, author and description. Only those values change in
`twproject.json`: your comments, your formatting and everything else in the file stay as they are.
Changing the id makes a new script — the one installed under the old name stays in the game until you
remove it.

**Remove from this list** forgets a project opened from elsewhere; its files stay where they are.
**Delete the project** moves a project's folder to the Recycle Bin. Neither touches what you have
installed into the game: that is the Scripts tab's business.

### AI coding agents

A project can be set up for an AI coding agent — **Claude Code**, **Codex** or **Cursor** — so that the agent
knows what it is working on: the scripting API, how the game is built inside, the tools, the rules, and that it
cannot see the game itself. Switch on **Set up for AI agents** when creating a project, or press **Set up for AI
agents** on any project, and pick the agents. The choice is remembered for the next project.

In practice that means the **Claude Code** or **Codex** extension for Visual Studio Code, or Cursor itself, opened
on the project from here. Command-line agents and standalone agent apps are not covered yet.

What it writes into the project:

| | |
|---|---|
| `AGENTS.md` | What every agent reads first. **It is yours**: write your own notes in it. The Tweaker only ever rewrites the part between `<!-- tweaker:begin -->` and `<!-- tweaker:end -->`, and adds that part to an `AGENTS.md` you already had. |
| `.agents/skills/` | Six skills: questioning a vague request before any code, the `tw` API and `twllink`, the game's files and what not to touch, CgrPy, checking a change in the game, and — only if you switch on **Binaries skill** — working with the game's native code. |
| `.tweaker/docs/` | A copy of this manual and of [How Audiosurf works inside](../internals.md), matching your Tweaker. |
| `.tweaker/manifest.json` | What of the above the Tweaker wrote. Commit it with the project. |
| `.tweaker/local.json` | Where the game and the Tweaker are on this computer. Rewritten every time the Tweaker opens the project, and added to `.gitignore`. |
| `CLAUDE.md`, `.claude/skills/` | Only with Claude Code, which does not read the files above by itself. `CLAUDE.md` is yours after it is written; it only pulls in `AGENTS.md`. |

**Updates.** A newer Tweaker may bring newer skills or docs; the project's row then says so and has **Update**.
Files you changed are kept as you left them, and the row lists them. **Use the kit's version** replaces them with
the Tweaker's and keeps yours next to each as a `.old` copy. A file you deleted stays deleted.

**What it does not do.** The Tweaker writes no settings of any agent and no MCP configuration, and installs
nothing; neither do the skills — when a tool is missing, the agent tells you what to install. The agent cannot
see the game: it builds and installs, and asks you to look. What it can read is the
[script log](getting-started.md#the-script-log).

### uv, for CgrPy

`cgrpy` needs [uv](https://docs.astral.sh/uv/), which the Tweaker does not ship. When it is missing, Script Dev
says so at the top, with the command to install it: `winget install --id=astral-sh.uv -e`. Restart the Tweaker
afterwards, so that the editors it opens find it.

## Scripts: what is installed

One row per `.lua` file in the game's `Scripts\` folder, with its name, file, version, author and
description, and a badge saying where it came from:

| | |
|---|---|
| **Bundled** | Comes with the Tweaker, unchanged. Updates keep it current. |
| **Bundled, edited** | Comes with the Tweaker and you changed it. The next update keeps your version as a `.old` copy and puts the bundled one back. |
| **Yours** | Not from the Tweaker. Updates never touch it. |
| **Project build** | Built by `twllink` from a project — its errors in the game point into the project's files, when its `.lua.map` is installed with it. |
| **Not installed** | Comes with the Tweaker but is not in the game: you deleted it, or it was never installed. |

**The switch** turns a script on or off. How depends on the game:

- **The game is not running**: the Tweaker writes the change into `scripts.cfg`, and the game uses it
  next time it starts.
- **The game is running and the Tweaker is connected to it**: the switch asks the game. It moves at
  once and says *Waiting for the game…* until the game confirms, which is usually immediate. If the
  game does not answer within a few seconds, the switch goes back and the Tweaker tells you. If the
  script fails to start, the switch goes back and the row says why.
- **The game is running, but the Tweaker is not connected** (overlay sync is off in Settings, or the
  plugin did not answer): the switches are locked and a banner says so. Switch the script in the
  game's overlay instead.

**The order of the rows is the order the scripts run in** — top first, the same list the overlay's
Scripts tab shows (see [Getting started § The order scripts run in](getting-started.md#the-order-scripts-run-in)
for why it matters). To change it, drag a row by the handle on its left (the six dots) and drop it where
the line shows. Like the switch, it depends on the game: with the game closed, the order goes into
`scripts.cfg`; with the game connected, the rows move at once and the game confirms (or the list goes back
after a few seconds, and the Tweaker tells you); with the game running but not connected, the handles are
locked. Bundled scripts that are not installed stay at the bottom and have no handle — they have no place
until they are in the game.

While connected, a row also shows what the game says about the script when there is something to say:
**Failed:** with the error, **Suspended:** with the reason (see
[Getting started § When something goes wrong](getting-started.md#when-something-goes-wrong)). The full
picture — cost, hooks, every message — is in the overlay's Scripts tab, in the game.

**Restore** (the circular arrow, on a row or for all of them) puts bundled scripts back the way this
version of the Tweaker ships them. A bundled script you deleted comes back; one you edited is replaced,
and your version is kept next to it as a `.old` copy — the row shows how many saved copies there are.
With the game connected, the game picks the restored scripts up at once.

**Refresh** reads the folder again. With the game connected, it also has the game look at its folder
again — the same as Rescan in the overlay — so a script you copied in by hand starts running.

## Next

[Projects: scripts made of many files](authoring.md) — what is inside a project, modules and
`require`, and every check `twllink` makes.
