# AgentKit

What Audiosurf Tweaker puts into a script project so that an AI coding agent — Claude Code, Codex or Cursor — can
work on it: an `AGENTS.md` block, six skills, and the pointers that tie them to the Tweaker's documentation and
tools. The Tweaker ships this folder as `DevTools\AgentKit\` next to `TweakerUI.exe`; its **Script Dev** tab copies
it into a project when asked to set the project up for agents.

This README is for whoever maintains the kit. What an agent reads is under `agents/` and `skills/`.

## Layout

```
kit.json                       the skill list (name, optional) and the kit's version
agents/
  agents-block.md              the managed block of AGENTS.md - what every agent reads first
  AGENTS.md                    AGENTS.md for a project that has none: {{project-name}}, {{block}}
  CLAUDE.md                    CLAUDE.md for Claude Code: @AGENTS.md
  claude-skill.md              Claude Code's pointer to a skill: {{name}}, {{description}}
skills/<name>/
  SKILL.md                     the skill: YAML front matter (name, description) + body
  references/*.md              detail the skill sends the agent to, read on demand
```

## What lands in a project

| From the kit | In the project | Owner |
|---|---|---|
| `agents/AGENTS.md` with the block filled in | `AGENTS.md`, if there is none | the user — written once |
| `agents/agents-block.md` | between `<!-- tweaker:begin -->` and `<!-- tweaker:end -->` in `AGENTS.md`; the markers are added at the end of a user's existing `AGENTS.md` that has none | the Tweaker |
| `skills/<name>/**` | `.agents/skills/<name>/**` — Codex and Cursor read skills from there | the Tweaker |
| — | `.tweaker/docs/**`, a copy of `DevTools\Docs` | the Tweaker |
| — | `.tweaker/manifest.json`: every Tweaker-owned file and its hash when written | the Tweaker |
| — | `.tweaker/local.json` (below), gitignored | the Tweaker, rewritten on every open |
| `agents/CLAUDE.md` | `CLAUDE.md`, only with Claude Code selected and only if there is none; an existing one gets `@AGENTS.md` added if it lacks it | the user |
| `agents/claude-skill.md` per skill | `.claude/skills/<name>/SKILL.md`, only with Claude Code selected — Claude Code reads skills only from there | the Tweaker |

A Tweaker-owned file the user has not changed (its hash still matches the manifest) is replaced by a newer kit
silently; one they changed is kept and reported; one they deleted is not brought back. `audiosurf-binaries` is
installed only when the user asks for it (`"optional": true`). Why this layout — and why Claude Code needs the
extra two files while Codex and Cursor need none — is in `Docs/Internal/dev-bundle-roadmap.md` (Ф0).

**Placeholders.** `{{project-name}}` is the project's `name` from `twproject.json`; `{{block}}` is `agents-block.md`.
In `claude-skill.md`, `{{name}}` and `{{description}}` are copied from the canonical skill's front matter, the
description **verbatim, as its YAML line** (it may contain characters that need the quoting it already has).

## `.tweaker/local.json`

The only place a project holds paths of this computer. Written by the Tweaker each time it opens the project,
gitignored, read by the skills:

```json
{
  "format": 1,
  "game": "D:\\SteamLibrary\\steamapps\\common\\Audiosurf",
  "devtools": "C:\\Tools\\Audiosurf Tweaker\\DevTools"
}
```

`game` is the Audiosurf folder (the one with `engine\`), `null` when the Tweaker does not know it. `devtools` holds
`Binaries\twllink.exe` and `CgrPy\cgrpy.cmd`.

## Writing for the kit

- **English**, for an agent that knows nothing about this game or this tool. Short sentences; the reason next to
  the rule.
- **One fact, one place.** A skill states what an agent must do and why, and sends it to `.tweaker/docs/` for the
  rest. Do not copy manual sections into skills; they drift.
- **Paths in the project's terms.** The docs are `.tweaker/docs/<path under Docs/>` — written in backticks,
  optionally with `#anchor`; the test checks each against `Docs/`. Other skills are named, not linked
  (`skill cgrpy`). No absolute paths anywhere.
- **Only real names.** Every `tw.*` name must exist in `Docs/scripting/api.json`, every `cgrpy <tool>` in
  `Tools/CgrPy/rdy2use/` — both checked by the test. A channel used in an example is one a shipped script already
  uses, or is marked as a lead to verify.
- **Front matter:** `name` equals the folder name, lower-case letters, digits and `-` (Cursor's rule); `description`
  says what the skill is *and when to use it*, in one line under 1 024 characters — it is all an agent sees when
  deciding whether to open the skill. The body stays well under 500 lines; detail goes to `references/`.
- **The rules are the owner's** (`dev-bundle-roadmap.md` §2): nothing installed by the agent, only the user's copy
  of the game, nothing from the game shared, the agent cannot see the game, cheating is named and then allowed.
  A skill may add to them, never soften them.
- **Bump `version` in `kit.json`** with any change to what lands in a project; Script Dev shows it.

## Checks

`TweakerLuaLinker.Tests` → `AgentKitTests`: the front matter of every skill, `kit.json` against the folders,
`tw.*` names and `cgrpy` commands in the kit and in `Docs/internals/`, `.tweaker/docs/` references, and relative
links and anchors in the public docs and the kit. Whether the skills actually work is checked the only way it can
be — an agent doing real tasks in a generated project (`dev-bundle-roadmap.md`, Ф6).
