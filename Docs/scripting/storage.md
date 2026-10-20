# Saving data

[← back to the index](../scripting.md)

Every script has **a folder of its own** to keep files in — its settings, a cache, a history of runs —
and seven functions to read and write them, as plain text or as JSON that comes back as Lua tables.

```lua
local cfg = tw.storage.read_json("settings.json") or {}     -- nothing on the first run: {}

cfg.runs = (cfg.runs or 0) + 1
tw.storage.write_json("settings.json", cfg)
```

This is the only way a script reaches the disk. `io` and most of `os` stay out of the environment
([Limits](limits.md#what-is-not-in-the-environment)): the folder is yours, the names are flat, and there
are limits on how much — enough for settings and caches, not a way into the rest of the machine.

---

## Where

```
engine/
  TweakerStuff/
    ScriptData/
      my-script/                <- for Scripts\my-script.lua
        settings.json
```

**One folder per script file**, named after the file without `.lua`, created the first time the script
writes. Reload, switching the script off and on, and rebuilding it with `twllink` under the same name all
see the same folder. A renamed script is a new script with a new, empty folder — move its files across
by hand if you rename one.

The player can open these files. JSON written here is indented and readable on purpose: a setting
people can fix in a text editor is a setting that never strands anyone.

## The functions

| | Returns |
|---|---|
| `tw.storage.read_text(name)` | the file's contents as a string, or `nil, reason` |
| `tw.storage.write_text(name, text)` | `true`, or `nil, reason` |
| `tw.storage.read_json(name)` | the value the file holds — usually a table — or `nil, reason` |
| `tw.storage.write_json(name, table)` | `true`, or `nil, reason` |
| `tw.storage.exists(name)` | `true` / `false` |
| `tw.storage.remove(name)` | `true`, or `nil, reason` |
| `tw.storage.list()` | the names of your files, sorted |

**Failures are results, not errors.** A file that is not there, a file that is not JSON, a value JSON
cannot hold, a limit — each returns `nil` and a one-line reason, so

```lua
local cfg, why = tw.storage.read_json("settings.json")
if cfg == nil then
    if tw.storage.exists("settings.json") then tw.notify("settings reset: " .. why) end
    cfg = {}
end
```

handles every case, and `read_json(...) or {}` handles the usual one. Only a mistake in the call
itself — a name that is not a string, `write_text` given a table — is a Lua error, at your line.

**Text is bytes.** `read_text` and `write_text` move the string as it is: any bytes, a zero byte
included, no line endings changed. A Lua string holds binary data as well as text.

## Names

Letters, digits, `_`, `.` and `-`, up to 64 characters, not starting with a dot: `settings.json`,
`runs-2026.csv`, `cache.v2`. No folders inside the folder, no `..`, no drive letters — a name that could
reach anything else cannot be spelled.

## Limits

| | |
|---|---|
| One file | 1 MiB |
| Files per script | 64 |
| The whole folder | 8 MiB |

A write that would go past one is refused with the reason; what is already there is not touched.
Rewriting a file counts its old size out first. Files the player put in the folder count too, if their
names are names a script could have used; anything else in the folder is ignored.

## When it reaches the disk

**A write returns at once.** The bytes go into memory, and the plugin's own thread writes them to disk
within a tenth of a second — through a temporary file that then replaces the old one, so a game that
crashes in the middle leaves either the old file or the new one, never half of either. Writing from
`on_frame`, every frame, is fine: what costs you is the copy into memory, and the disk sees only the
latest version, ten times a second at most.

**When your script is switched off or reloaded**, whatever it wrote is written there and then. When the
game itself is closed, a write made in the last tenth of a second before it may not make it — so save on
the change, not at exit (`on_unload` does not run when the game quits).

**A read comes from memory too** — except the first read of a file that is already on disk, which reads
it. That first read is a real file read, so read your settings **while the script loads**, at the top
level, not on a frame the player is watching. After that, reading the same file again is a copy of a
string, and reading what you just wrote gives you what you wrote, whether or not it has reached the disk
yet.

The first use of `tw.storage` after the script loads also looks at what is in the folder, once — the
limits count what is there.

**Edited by hand while the game runs?** Your script sees the change after it is reloaded: a reloaded
script starts with nothing in memory and reads the disk again.

## JSON

`read_json` reads the file and turns it into Lua values; `write_json` does the reverse. Both are done by
a proper JSON library inside the plugin, so escaping, Unicode and number formatting are right without
your script doing anything.

| JSON | Lua | Written back as |
|---|---|---|
| object | a table with string keys | a table whose keys are all strings |
| array | a table with keys `1..n` | a table whose keys are exactly `1..n` |
| `{}`, `[]` | an empty table | an empty table is written as `{}` |
| string | string | |
| `true`, `false` | boolean | |
| number | number — whole numbers past 2^53 lose precision | whole numbers without a fraction (`3`, not `3.0`) |
| `null` | `nil`: the key is simply not there, or a hole in an array | |

Keys are written **sorted**, so a file does not reshuffle itself between two saves and stays easy to
compare. Reading is forgiving about what people leave in a file by hand — a byte-order mark, `//`
comments, a trailing comma — and about nothing else.

`write_json` refuses, with the reason, a value JSON has no room for:

- `NaN` or an infinity;
- a table mixing string keys with numbered ones, an array with a hole in it, a key that is neither a
  string nor part of `1..n` (`1.5`, `0`, `true`);
- a function, a coroutine, userdata;
- a table that contains itself, or tables nested deeper than 64;
- a string that is not valid UTF-8.

`read_json` of a file that holds `null` returns `nil, "<name> holds null"`. Anything else at the top of
a file — a number, a string, an array — comes back as that value.

---

## Next

[Worked examples](examples.md) — probes taken apart line by line, the last of them a settings tab that
keeps its values here. Every function on one page: [API reference § Storage](api-reference.md#storage).
