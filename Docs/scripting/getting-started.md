# Getting started

[← back to the index](../scripting.md)

## Where scripts live

Scripts are plain `.lua` files, inside your Audiosurf installation. Everything the Tweaker plugin
reads or writes lives under `engine\TweakerStuff\`:

```
Audiosurf/
  engine/
    QuestViewer.exe             <- the game; everything below is found relative to it
    channels/
      TweakerPlugin.dll         <- the plugin itself, loaded by the game at startup
    TweakerStuff/
      Scripts/
        hello.lua
        puzzlepro_hud.lua
        particles.lua
        my-script.lua           <- yours
      Config/
        scripts.cfg             <- created automatically; remembers which scripts you turned off
      SkyboxReplacer/           <- the Skybox Replacer's own folder, see Docs/skyboxes.md
      Logs/
```

They are loose files on purpose, not packed into the plugin. You can edit one in a text editor and
reload it without rebuilding anything, and sharing a script means sending someone a file.

Every `.lua` file directly in `Scripts/` is picked up. **Subfolders are not scanned**, so a folder
is a fine place to park scripts you do not want running.

## Getting the plugin in there

The usual way is Audiosurf Tweaker: **Settings → In-game plugin → Install the plugin into
Audiosurf**. It copies the plugin and the bundled scripts into the layout above, and keeps them up
to date when you update the Tweaker. Anything you wrote yourself it leaves alone; if you edited one
of the bundled scripts, an update keeps your version next to the new one as `<name>.lua.old`.

**You do not need Audiosurf Tweaker running to use scripts.** The game loads the plugin by itself at
startup, so scripts, skyboxes and the overlay work with nothing else running. The Tweaker is only
needed for the overlay's Skins, Tweaks and Player tabs, which are its own features — without it
those tabs show as offline and everything else carries on.

You can also install by hand: copy `TweakerPlugin.dll` into `engine\channels\` and put your `.lua`
files in `engine\TweakerStuff\Scripts\` (the plugin creates the folders on first run). In a Tweaker
release both live under `PluginPayload\`, laid out exactly as above.

If you ever want the plugin to sit out a session without uninstalling it, create an empty file
called `DISABLE` in `engine\TweakerStuff\`. The plugin checks for it before doing anything at all.

## Your first script

Create `engine\TweakerStuff\Scripts\first.lua`:

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
   loads, which is long before the game has a run going — and, with the Tweaker loaded by the game
   itself, before the game has loaded anything at all. So the top level declares; it never reads.
   Your handlers do not start until the game has finished loading, which is why you will not find a
   "has it loaded yet?" check anywhere in this guide.
2. **`tw.on_frame` registers a function to run every drawn frame.** This is where drawing happens —
   and only drawing. Work that reads channels and computes belongs in **`tw.on_tick`**, which runs
   on the *game's* frame rather than the overlay's. The two are different rates: see
   [API reference § Lifecycle](api-reference.md#lifecycle). A small script can do everything in
   `on_frame` and be fine; a script that counts things cannot.
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
- a **Reload** button, on running scripts — re-runs the file from disk;
- a **state**: `running`, `waiting` (the game is still loading, a group it hooks is not loaded yet, or
  it said [`tw.pending`](api-reference.md#twwarnmessage)), `suspended`, `failed` or `off`;
- its **cost**, when it is high enough to mention — the average time its handlers take per frame;
- how many channel **hooks** it holds, and how many of those are still waiting for their group;
- its **messages** — click to unfold: every warning and error it produced, each with a count, and the
  full text with the traceback when you hover one;
- a **Resume** button, on a suspended script.

At the top: what the game is doing (loading, ready, loading a run). At the bottom: how many scripts
are running and how many channel hooks they hold between them.

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

`TweakerStuff\Config\scripts.cfg` stores **only the scripts you switched off**. Anything not listed
is on. So a new `.lua` dropped into the folder runs immediately, which is what you want when someone
sends you one, and deleting a script leaves at worst a stale line naming a file that no longer
exists.

## When something goes wrong

### The script has a syntax error, or throws while loading

The row in the Scripts tab gets a red border and shows the error instead of the description, and the
toggle springs back to off. Fix the file and toggle it on again.

### The script throws while running

You get one notification naming the script and the error, and the error goes into the script's
messages in the Scripts tab, with a counter. **Nothing else stops**: the handler that threw loses
that one call, and every other handler — yours and every other script's — carries on.

If it keeps happening — five errors within a few seconds — the script is **suspended**: its row
turns amber and says why, its handlers stop running, and anything it muted in the game comes back.
Other scripts are not affected. Fix the file and hit **Reload**, or hit **Resume** to let it carry on
from where it was.

A script whose handlers take too long, frame after frame, is suspended the same way; see
[Limits § Failure containment](limits.md#failure-containment).

### A script cannot find something

If a script asks for a channel that does not exist, it is an error of that script, pointing at the
line that asked:

```
Lua: My HUD: StatCollector.Pointz: no such channel in group
```

It is reported **once**, and it means what it says: the group *is* loaded and has no channel by that
name. A group's channel list is fixed the moment it loads, so this can never come right by waiting —
it is a typo or a wrong name, never a timing problem, and the handle stops looking.

The other case — the group itself not being loaded — is silent, because it is normal. The groups of a
run do not exist in the menu and never will until you start one. Your handle waits, and resolves
itself when the group turns up.

If you asked for something through the *wrong kind* of accessor — a number accessor on a text
channel — that is a hard error immediately, naming both types. It can never fix itself, so it is not
worth waiting on.

### Nothing happens at all

Check, in order:

- Is the script listed in the Scripts tab? If not, it is not directly in
  `engine\TweakerStuff\Scripts\` (a subfolder will not do), or does not end in `.lua`.
- Is its toggle on?
- Are you in a run? Most of the interesting channels only exist while the game is actually playing.
- Does anything draw? Try `tw.notify("alive")` at the top level — it shows a toast the moment the
  script loads.

`print(...)` works and goes to the plugin log. `tw.notify(...)` shows a toast on screen. In release
builds the log is stripped, so `tw.warn(...)` — which files the message in your script's row in the
Scripts tab, with a counter — is the one to reach for when you need to see something in a normal
install.

## Next

[How Audiosurf is built](game-model.md) — the mental model everything else rests on.
