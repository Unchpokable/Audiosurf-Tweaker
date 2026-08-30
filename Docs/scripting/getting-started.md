# Getting started

[← back to the index](../scripting.md)

## Where scripts live

Scripts are plain `.lua` files in a folder called `scripts/`, sitting next to `TweakerPlugin.dll`:

```
TweakerUI/
  TweakerPlugin.dll
  scripts/
    hello.lua
    traffic.lua
    my-script.lua        <- yours
  TweakerScripts.cfg     <- created automatically; remembers which scripts you turned off
```

They are loose files on purpose, not packed into the plugin. You can edit one in a text editor and
reload it without rebuilding anything, and sharing a script means sending someone a file.

Every `.lua` file directly in that folder is picked up. Subfolders are not scanned.

## Your first script

Create `scripts/first.lua`:

```lua
-- @name        First
-- @description Prints the song timer

local timer = tw.float_ch("StatCollector", "Timer")

tw.on_frame(function()
    local t = timer:get()
    if t == nil then return end

    local x, y = tw.hud.safe()
    tw.hud.text(x + 20, y + 20, string.format("%.1f s", t))
end)
```

Start Audiosurf with the Tweaker, start a run, and the song timer appears in the corner.

Three things are happening, and they are the shape of nearly every script:

1. **At the top level, you declare what you need.** `tw.float_ch(...)` does not read anything yet —
   it creates a *handle* that will resolve itself later. Top-level code runs once, when the script
   loads, which is usually long before the game has a run going.
2. **`tw.on_frame` registers a function to run every frame.** This is where the work happens.
3. **Everything can be `nil`.** `timer:get()` returns `nil` until the channel is actually reachable.
   Scripts that assume otherwise crash the moment you open a menu. See
   [Reading and writing the game § Nothing is available immediately](channels.md#nothing-is-available-immediately).

There is no `require`, no imports, and no boilerplate. The `tw` table is already there.

## The script header

The first comment block of a file can carry metadata, which the Tweaker reads **without running the
script** — that is how the Scripts tab can list something you have turned off:

```lua
-- @name        Run Tracker
-- @author      Your Name
-- @version     1.2
-- @description Traffic taken off the road, live bonuses, and a medal progress bar
```

All four are optional. Missing `@name` falls back to the file name. Scanning stops at the first line
that is neither blank nor a comment, so put the header at the very top.

Keep `@description` to one line — it is drawn on one line and clipped.

## The Scripts tab

Press **Insert** in game to open the overlay menu, and go to **Scripts**.

Every `.lua` file in the folder is listed, whether it is running or not, with its name, version,
author and description. Each row has:

- a **toggle** — turns the script on and off;
- a **Reload** button, on running scripts — re-runs the file from disk.

At the bottom: how many scripts are running and how many channel hooks they hold between them.

### Turning a script off really turns it off

This is worth knowing because it shapes how you write scripts.

Disabling does not pause anything. It **removes** the script: its frame handlers, its subscriptions,
and the interception the Tweaker installed in the game for it. A disabled script costs the game
nothing at all — not a check, not a branch. The game goes back to running exactly as if the script
had never loaded.

Which means **enabling is a fresh start**, not a resume. The file is read from disk and run again,
with an empty environment. Any state your script accumulated is gone.

Two consequences:

- **Reload is free, and it is how you iterate.** Edit the file, hit Reload (or toggle off and on),
  and your changes are live. No restarting the game.
- **Do not rely on state surviving a toggle.** If your script counts something, it starts from zero.
  That is usually what you want anyway.

You can toggle a script mid-run. The game handles it.

### Which scripts are on is remembered

`TweakerScripts.cfg` next to the DLL stores **only the scripts you switched off**. Anything not
listed is on. So a new `.lua` dropped into the folder runs immediately, which is what you want when
someone sends you one, and deleting a script leaves at worst a stale line naming a file that no
longer exists.

## When something goes wrong

### The script has a syntax error, or throws while loading

The row in the Scripts tab gets a red border and shows the error instead of the description, and the
toggle springs back to off. Fix the file and toggle it on again.

### The script throws while running

The first time a frame handler throws, **script drawing is switched off** and you get a notification
with the error. It is not retried: a handler that throws once throws sixty times a second, and a
screen full of the same error is worse than no error.

The same applies to channel hooks, separately.

To recover: fix the file and toggle any script off and on. Toggling clears the latch.

### A script cannot find something

If a script asks for a channel that does not exist, you get one notification naming it:

```
Lua: StatCollector.Pointz: no such channel in group
```

It is reported **once**, and only after several failed attempts — because "not there yet" and "never
going to be there" look identical at first, and the game genuinely does load and unload parts of
itself as you move between menus and runs. If you see this message, it is a typo or a wrong group
name, not a timing problem.

If you asked for something through the *wrong kind* of accessor — a number accessor on a text
channel — that is a hard error immediately, naming both types. It can never fix itself, so it is not
worth waiting on.

### Nothing happens at all

Check, in order:

- Is the script listed in the Scripts tab? If not, it is not in `scripts/` or does not end in `.lua`.
- Is its toggle on?
- Are you in a run? Most of the interesting channels only exist while the game is actually playing.
- Does anything draw? Try `tw.notify("alive")` at the top level — it shows a toast the moment the
  script loads.

`print(...)` works and goes to the plugin log. `tw.notify(...)` shows a toast on screen. In release
builds the log is stripped, so `tw.warn(...)` — which does both — is the one to reach for when you
need to see something in a normal install.

## Next

[How Audiosurf is built](game-model.md) — the mental model everything else rests on.
