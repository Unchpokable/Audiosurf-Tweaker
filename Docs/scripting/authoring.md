# Projects: scripts made of many files

[← back to the index](../scripting.md)

A script is one `.lua` file, and for a small one that is exactly right. A HUD that reads half the
game is not small. `puzzlepro_hud.lua` was one file of 2 100 lines; it is now 17 files of 20–250 lines
each, and what the game loads is still one file.

The tool that turns a folder of modules into that one file is **`twllink`** — the Tweaker's Lua
linker. It ships with the Tweaker, as `twllink.exe` in the `DevTools\Binaries\` folder next to
`TweakerUI.exe`, and needs nothing installed to run.

**You do not have to use it from a terminal.** The Tweaker's **Script Dev** tab drives it for you:
it creates projects, builds and installs them, rebuilds when you save, lists the errors and opens your
editor on them, and opens the editor with `twllink` already on its `PATH`. That is the easy way in, and
it is described in [Scripts in Audiosurf Tweaker](tweaker.md). This page is about what a project *is* —
the same whichever way you build it — and about `twllink` itself, for when you want the command line.
To type `twllink` in a terminal of your own, put `DevTools\Binaries` on your `PATH`.

Nothing about the game side changes. The plugin never learns there were several files: what it loads
is ordinary Lua, the same as a script written by hand, with the same rights and the same limits.

## Starting a project

In the Tweaker: **Script Dev → +**, a name, a template, **Create** — see
[Your first project](tweaker.md#your-first-project). From a terminal:

```
twllink --new hud my_hud
```

creates `my_hud/` with a project that builds as it is. Both make the same project. Three templates:

| Template | |
|---|---|
| `hud` | Reads a channel and draws it, with a second module to show how `require` works. |
| `hook` | Counts something the game does, through `tw.on_call`, and draws the count. |
| `empty` | `main.lua` with an empty `tw.on_frame`. |

The folder must be new or empty. With `--new`, the script's `id` is made from the folder name (`My
HUD!` becomes `my_hud`); Script Dev makes it from the name you type, and lets you change it right
there. Either way, change it before anyone else has the script, not after — see below.

### Your editor

A new project is set up for [Lua Language Server](https://luals.github.io/) — in VS Code that is the
**Lua** extension by sumneko. Open the project folder, and:

- every `tw.*` name completes as you type, and hovering one shows its parameters and its description
  from the [API reference](api-reference.md);
- a name that does not exist — `tw.hud.txt` — is underlined before you ever build;
- `io`, `debug`, `ffi`, `jit` and `package` are unknown to the editor, exactly as they are to the game;
- `require("draw.rail")` finds `src/draw/rail.lua`;
- `twproject.json` completes and checks its own fields.

That comes from three files `--new` writes: `.luarc.json` (the editor's settings), and `.twllink/`
holding `tw.def.lua` (the API, as definitions) and `twproject.schema.json`. The last two are copies of
what your `twllink` knows, and every build refreshes them — so a newer Tweaker brings its new names
into your editor by itself. Delete `.twllink/` and they are not recreated; nothing in the build needs
them.

The same `tw.def.lua` is in the Tweaker's repository as `Docs/scripting/tw.def.lua`, for scripts that
are not projects.

## A project

```
my_hud/
  twproject.json        the manifest - always this name
  src/main.lua          the entry point - always this name
  src/palette.lua       require("palette")
  src/draw/rail.lua     require("draw.rail")
  docs/NOTES.md         anything else: part of the project, not of the script
  LICENSE
```

Two names are fixed, on purpose:

- **`twproject.json`** is how a folder is recognised as a project — by `twllink`, and by the Tweaker's
  Script Dev tab when you open a folder as a project.
- **`src/main.lua`** is where the script starts. There is no setting for it, the same way a C program
  has no setting for `main`.

Every other file under `src/` is a module, named by its path: `src/draw/rail.lua` is `draw.rail`.

### There is no list of source files

`twllink` starts at `main` and follows `require` calls. What `main` requires is in the script, what
those require is in it, and so on; anything nobody requires is not. You never list your files, and a
file cannot sneak into the build by sitting in the folder.

The flip side is reported rather than hidden: a `.lua` file under `src/` that nothing reaches gets a
warning (TW0109). That is how a misspelt module name, or a file you forgot to wire in, shows up.

## `twproject.json`

```jsonc
{
  "format": 1,
  "id": "my_hud",
  "name": "My HUD",
  "author": "you",
  "version": "1.0.0",
  "description": "What it does, on one line.",
  "api": 2,

  // Everything below is optional; these are the defaults.
  "build": {
    "comments": "keep",
    "embed": []
  },
  "diagnostics": {
    "warnings_as_errors": false,
    "suppress": []
  }
}
```

Comments and trailing commas are allowed.

| Field | Meaning |
|---|---|
| `format` | The manifest format. Always `1` for now. |
| `id` | Lower-case letters, digits and `_`, up to 64. The built script is `<id>.lua`. **The id is the script's identity to the plugin** — the Scripts tab remembers "switched off" by file name — so do not change it once people have your script: a renamed file is a new script, and the old one stays installed next to it. |
| `name` | What the Scripts tab shows. Change it freely. |
| `author`, `description` | Shown in the Scripts tab. |
| `version` | **`MAJOR.MINOR.PATCH`, strictly**: three whole numbers, no leading zeros, no `-beta`. |
| `api` | The version of the `tw` API the script is written against. `2` today: API 2 added `tw.hud.occupied`, `tw.hud.claim` and `tw.hud.place`. `twllink --new` writes the current one. |
| `build.comments` | `"keep"` or `"strip"`. See [Comments](#comments). |
| `build.embed` | Text files from the project to carry inside the script as comments — see [Embedding files](#embedding-files). |
| `diagnostics.warnings_as_errors` | Fail the build on any warning. |
| `diagnostics.suppress` | Warning codes to silence, e.g. `["TW0109"]`. Errors cannot be suppressed. |

`name`, `author` and `description` go into the script's header, which is one line per field — so
they cannot contain line breaks.

**The manifest describes the project, not your computer.** There is nothing in it about where the
game is installed, and every path is inside the project folder. A project folder can be zipped and
sent to someone else as it is.

## Modules and `require`

A module is a file whose top level runs once, the first time something requires it, and whose
`return` value is what `require` hands back — the same as in ordinary Lua:

```lua
-- src/palette.lua
local M = {}

local fallback = 0xFFE0E0E0          -- local to this module

function M.colour_of(id)
    return fallback
end

return M
```

```lua
-- src/main.lua
local palette = require("palette")
local rail = require("draw.rail")

tw.on_frame(function()
    rail.draw(palette.colour_of(3))
end)
```

The rules, all of them checked when you build:

- **The name is a string literal.** `require("draw.rail")` and `require "draw.rail"` are fine;
  `require(name)` is an error (TW0103). The linker has to know every module before the script ever
  runs — there is no disk to load one from later.
- **Dots, not slashes.** `draw.rail`, never `draw/rail` (TW0105).
- **Case matters**, even though Windows does not care: `require("Palette")` for `palette.lua` is an
  error (TW0106). Two spellings would load the same file twice, with two copies of its state.
- **No cycles.** `a` requiring `b` requiring `a` is an error (TW0107), with the whole loop in the
  message. At runtime the second `require` would get a half-loaded module; the build refuses instead.
- **`main` cannot be required** (TW0108). Put what two files share into a module of its own.

And what a module can count on:

- **Its top-level locals stay its own.** A module is compiled as a function, so a `local` at its top
  level is local to that file, not to the whole script.
- **It runs once.** Every `require` of the same name returns the same value. A module that returns
  nothing is `true` to whoever requires it, as in standard Lua.
- **`...` is its module name**, as in standard Lua.
- **Order is the order you require in.** Modules run the first time they are required, not before.
  `main` runs everything else, directly or not — so the order of the `require` lines in `main` is the
  order their top-level code runs in. That matters for anything with a side effect: two `tw.on_call`
  handlers on the same channel run in the order they were registered. This is also the way to fix the
  order between two parts of yours: between separate scripts, the order is the user's
  ([API reference § Lifecycle](api-reference.md#lifecycle)).

Registration (`tw.on_frame`, `tw.on_call`, `tw.mute`, …) works at the top level of any module, not
only in `main`: a module's top level runs while the script is loading, which is exactly when
registration is allowed ([API reference § Lifecycle](api-reference.md#lifecycle)).

**Globals are still global.** Every module shares the script's environment. A variable assigned
without `local` in one module is visible in every other one — and in the finished script, exactly as
if it were one file. Use `local`, and pass things through `require`; the build warns about every global a
module assigns (TW0305).

## Building

Open a terminal in the project folder and run:

```
twllink
```

```
twllink: my_hud 1.0.0 -> build/my_hud.lua (3 modules, 212 lines)
  modules: palette, draw.rail, main
```

That writes two files into `build/`: **`my_hud.lua`**, the script, and **`my_hud.lua.map`**, which
tells the plugin where each line came from. Files are only rewritten when their content changes.

| Option | |
|---|---|
| `--project <folder or twproject.json>` | Build a project other than the one in the current folder. |
| `--out <file.lua or folder\>` | Write somewhere other than `build/`. The map goes next to the script. |
| `--install` | Also copy the script and its map into the game — see [Installing](#installing). |
| `--game <folder>` | The Audiosurf folder for `--install`. |
| `--json` | Print the result as JSON instead of text — for tools. |
| `--api <api.json>` | Check against another description of the API than the one `twllink` came with. |
| `--new <template> <folder>` | Create a project — see [Starting a project](#starting-a-project). |
| `--version`, `--help` | |

Exit codes: **0** built, **1** the project has errors, **2** the command line was wrong, **3** `twllink`
itself failed. A build that fails writes nothing — and installs nothing.

Script Dev's **Build** and **Build and install** run exactly this, and show the same messages as a list.

### Installing

```
twllink --install
```

builds, then copies **both files** into the game's `engine\TweakerStuff\Scripts\`. The game is found
through `--game <folder>` or the `AUDIOSURF_DIR` environment variable — either the Audiosurf folder
or its `engine\` folder. The Tweaker's plugin has to be installed in that game already (there has to
be an `engine\TweakerStuff\`); `twllink` does not install the plugin.

Where your game is, is a fact about your computer, so it never goes into `twproject.json`.

By hand works just as well: copy both files. The script runs without the map; the map is what makes
its errors readable.

If the game is running, it does not notice the new file by itself: press **Reload** on the script's
row in the overlay's Scripts tab (or switch it off and on) to run the new version, or **Rescan** if the
script was not installed before. **Build and install** in the Tweaker's Script Dev tab does that for
you: with the game running and the Tweaker connected, the game reloads the script the moment it is
installed.

`twllink` builds once per run and does not watch your files: after a change, run it again. Script
Dev's **Rebuild on save** does exactly that — and, with **and install into the game**, installs and
reloads too — see [Rebuild on save](tweaker.md#rebuild-on-save).

## Checks at build time

`twllink` knows exactly what the game gives a script: every global, every `tw.*` name, and what the
[sandbox](limits.md#what-is-not-in-the-environment) takes away. That list is not written by hand — it
is taken from the plugin itself when the Tweaker is built — so it is right for the Tweaker your
`twllink` came with. Code that reaches for something that is not there is reported at build time,
with the file and the line, instead of failing in the game:

```lua
local f = io.open("notes.txt")        -- error TW0301: 'io' is not available to scripts
tw.hud.txt(20, 20, "hi")              -- error TW0302: 'tw.hud.txt' does not exist in API 2 - did you mean 'tw.hud.text'?
tw.state_cache = {}                   -- error TW0304: writes into the tw every script shares
counter = 0                           -- warning TW0305: assigned without 'local'
local n = mathh.floor(1.5)            -- warning TW0306: 'mathh' is not a global
local s = string.formatt("%d", 1)     -- warning TW0307: 'string.formatt' does not exist
```

- **The sandbox and `tw.*` are errors.** A removed library is not something a script can work
  around, and a misspelt `tw` name is a nil call waiting for the right frame.
- **`tw` is shared by every script.** Putting your own values into it changes what every other
  script sees. Keep them in locals, or in a table of your own.
- **A global assignment is a warning**, once per name. It is legal Lua, and sometimes the point — but
  it is shared by every module of the script, so it is worth saying out loud. A global that some
  module assigns is not "unknown" to the others.
- **A missing field of a standard library is a warning**: code that tests for a feature
  (`if table.move then`) is allowed to ask about something that is not there.

Only plain names are checked: `tw.hud.text`, `os.execute`. A handle's methods (`handle:get()`) and
fields reached through a variable (`local hud = tw.hud; hud.txt()`) are not — the editor, with
`tw.def.lua`, catches most of those as you type.

### API versions

`"api"` in `twproject.json` is the version of the `tw` API your script is written against;
`tw.api_version` in the game is the version the Tweaker provides. The built script carries yours in
its header (`-- @api 2`), and a Tweaker with an older API **does not run the script** — its row says
which version it needs — instead of letting it fail on the first name that version does not have.

A name added in a later API than your project's `"api"` is a warning (TW0303): using it means the
script needs that later version, so raise `"api"` — or do without it. A project whose `"api"` is newer
than your `twllink` knows is an error (TW0016): update the Tweaker.

## Errors point at your files

A runtime error in a built script happens at a line of the one big file. With the map next to it, the
plugin turns that back into your file and your line before it shows anything:

```
error  draw/rail.lua:41: attempt to index a nil value (local 'colour')
```

— in the message, in every line of the traceback in its tooltip, and in the place a `tw.warn` was
said from. Positions are relative to `src/`, so `draw/rail.lua:41` is line 41 of `src/draw/rail.lua`.

The map belongs to one exact build of the script. **If you edit the installed `.lua` by hand**, the
map no longer matches it, and the plugin ignores it rather than point at the wrong lines — you get
one message in the script's row saying so, and positions in the script's own lines. Rebuild and
install both files again, and it is back.

A script without a map — every script written by hand — is not affected by any of this.

## Comments

`"comments": "keep"` (the default) copies every module into the script exactly as written, comments
and all. The built script stays readable, and for a script people will share, that is usually what
you want.

`"comments": "strip"` removes them. A comment on a line of its own goes with its line; a comment
after code goes and leaves the code. Strings that happen to contain `--` are not comments and are
left alone. The header the plugin reads is never stripped, and neither is anything embedded.

Error positions stay exact either way — the map records the lines that were actually written.

## Embedding files

```jsonc
"build": { "embed": ["LICENSE", "docs/NOTES.md"] }
```

Each file listed is carried inside the script as a Lua long comment, near the top. It does nothing
when the script runs; it is there so that whoever receives the one `.lua` file receives it too.

A licence is the obvious case: the script is what people pass around, and a licence that stays behind
in your project folder protects nothing. Design notes are the other one — `puzzlepro_hud` carries its
own this way.

Embedded files must be UTF-8 text inside the project folder.

## What the built script looks like

You do not need to know this, but it is ordinary Lua and you are welcome to read it:

```lua
-- @name        My HUD
-- @author      you
-- @version     1.0.0
-- @description What it does, on one line.
-- @api         1
-- @bundle      twllink 1.1, 3 modules

local __twl_modules, __twl_cache, __twl_loading = {}, {}, {}

local function require(name)
    ...                                   -- runs each module once, caches what it returns
end

__twl_modules["palette"] = function(require, ...)
... src/palette.lua, line for line ...
end

__twl_modules["main"] = function(require, ...)
... src/main.lua ...
end

require("main")
```

`require` is a local of the script, not the global the sandbox removes — a built script has exactly
the rights of one written by hand, and nothing is read from disk at runtime. Module bodies are not
indented, so each line of a module is in the script as you wrote it (less its comments, with `strip`).

## Diagnostics

Every message has a code. Codes are never reused, so one you look up here means the same thing in
every version.

| Code | | Meaning |
|---|---|---|
| TW0001 | error | No `twproject.json`. |
| TW0002 | error | `twproject.json` is not valid JSON, or not an object. |
| TW0003 | error | A manifest `format` this `twllink` does not know — the project is newer than the tool. |
| TW0004 | error | A required field is missing. |
| TW0005 | error | A field has the wrong type. |
| TW0006 | error | `id` is not lower-case letters, digits and `_`. |
| TW0007 | error | `version` is not `MAJOR.MINOR.PATCH`. |
| TW0008 | error | `name`, `author` or `description` is empty or contains a line break. |
| TW0009 | error | `api` is not a positive whole number. |
| TW0010 | warning | An unknown field — usually a typo. It is ignored. |
| TW0011 | error | `build.comments` is not `"keep"` or `"strip"`. |
| TW0012 | error | A file in `build.embed` does not exist. |
| TW0013 | error | A file in `build.embed` is outside the project folder. |
| TW0014 | error | A file in `build.embed` is not UTF-8 text. |
| TW0015 | warning | `diagnostics.suppress` names an error or a code that does not exist. |
| TW0016 | error | `api` is newer than this `twllink` knows — update the Tweaker. |
| TW0101 | error | There is no `src/main.lua`. |
| TW0102 | error | A syntax error. The parser's own code is in brackets at the end. |
| TW0103 | error | `require` with anything but one string literal. |
| TW0104 | error | A required module has no file; the message names the file it looked for. |
| TW0105 | error | Not a module name — use dots and letters, digits, `_`. |
| TW0106 | error | The module name and the file name differ in case. |
| TW0107 | error | A `require` cycle; the message shows the whole loop. |
| TW0108 | error | `require("main")`. |
| TW0109 | warning | A `.lua` file under `src/` that nothing requires. It is not in the script. |
| TW0110 | error | A module is not valid UTF-8 or cannot be read. |
| TW0201 | error | The script could not be written. |
| TW0202 | error | The built script does not parse. This is a bug in `twllink`, not in your project — please report it with the project. |
| TW0203 | error | `--install` could not: no game given, not an Audiosurf folder, the plugin not installed in it, or the copy failed. The build itself succeeded. |
| TW0301 | error | Something the sandbox removes: `io`, `debug`, `os.execute`, … — see [Limits](limits.md#what-is-not-in-the-environment). |
| TW0302 | error | A `tw.*` name that does not exist, with the closest real one when there is one. |
| TW0303 | warning | A `tw.*` name from a newer API than the project's `"api"`. |
| TW0304 | error | A write into `tw` — it is shared by every script. |
| TW0305 | warning | A global assigned without `local`. Once per name. |
| TW0306 | warning | A global that is neither in the script environment nor assigned by any module. Once per name. |
| TW0307 | warning | A field of a standard library that does not exist (`string.formatt`). |

Syntax is checked as **LuaJIT 2.1**, the Lua the game runs: `goto`, `0xFFULL` and `\u{...}` are fine;
Lua 5.3's `//` and `&` are errors, because the game would reject them too.
