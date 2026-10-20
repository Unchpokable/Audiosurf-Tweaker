---
name: audiosurf-grill
description: Interrogating a request before any code - turns "a beautiful HUD with fireworks" or "make Eraser Elite's undo push with overflow and burn the matches" into a short, confirmed spec in Audiosurf's own terms, with a verdict on whether a script can do it at all. Use for the first request in this project, for any vague, aesthetic or game-jargon request, for anything that changes gameplay or scoring, and whenever you would otherwise have to guess what the user means.
---

# Grill first, code second

Many requests here come from players, not programmers: "I want a nice HUD", "fireworks when I match", "make undo
work like it used to". They know the game far better than you and the code not at all. Your job before any code
is to find out **what exactly they want to see in the game**, whether a script can do it, and to get a yes on a
short written spec. A wrong guess costs the user a round in the game for nothing — and you cannot see the game to
notice.

**Read `.tweaker/docs/internals/playing-audiosurf.md` before the first question.** It is the game from the
player's seat — screens, characters, abilities, the board, scoring, the HUD, the words players use — with every
term tied to where it lives in the game. Your questions should use the game's words, not yours.

## When

- the first request in this project;
- a request about looks or feel ("beautiful", "cleaner", "like in osu!", "fireworks");
- game jargon you cannot map with certainty ("стакан", "чейн", "Undo", "Butler", a character's name);
- anything that would change how the game plays or scores;
- any time you notice you are about to assume something the user did not say.

Not for a precise change in a conversation that already has a spec ("rename this", "move it 10 px left").

## How

1. **Retell it in the player's terms**, one short paragraph: what they will see, when, where. "So: during a
   Puzzle run, next to the score, a counter of blocks per colour, replacing the game's '+N'?" — then the questions.
2. **Ask in rounds, with defaults.** Three to six questions a round, each with the answer you would pick if they
   do not care: *"Which characters? (default: all Puzzle characters)"*. The user may answer "defaults" to the whole
   round. The checklist is in `references/questions.md` — pick what is unclear, do not recite it. Most requests
   settle in one or two rounds; past three, write the spec with the remaining defaults marked as assumptions.
3. **Match the person.** A player who has never written code hears no "channel", "hook", "vtable" or "API" — ask
   about the screen and the game. Someone who talks in code gets code-level questions. Answer in the language they
   wrote in.
4. **Never guess a word.** A term you cannot map through the reference — ask what they mean, with your best guess
   as the default. Two different things can share a word ("auto" in Freeride is the autopilot or AutoAdvance).
5. **Numbers from memory are hypotheses.** Players remember the game well and numbers badly (a stack of 2/3/4 that
   is 1/3/4 in the game). Check every number against the game (skill `cgrpy`) and show the user any difference
   instead of quietly picking one.
6. **Find out before promising.** Look it up — the reference, `.tweaker/docs/internals/gameplay-map.md`,
   `cgrpy query` — before saying "can do". The verdict comes after that, not from intuition.
7. **The cheating question**, when it applies (below).
8. **Write the spec, wait for yes.** Template below. No code before the user agrees with it — a quick throwaway
   probe to answer a feasibility question is the only exception, and you say it is one.

## Do not reason the game out — look it up

A one-line summary of a mode ("Mono: collect the coloured, dodge the grey") is not its rules, and what follows from
it "logically" is often false. Before telling the user how something in the game works — or that something does
not apply to them — find it in the reference or the gameplay map, and when neither says, ask the graph or ask the
user. Things agents have got wrong this way:

- **"Mono has no matches, so the chain does not matter."** False. Mono has the board, matches and the chain like
  Puzzle — every coloured block matches every other — and on top of that pays for each coloured block as it is
  collected. The chain matters in Mono (reference, "Scoring, as players think of it").
- **"Collected blocks per colour is one hook on the collection handler."** It is two handlers and a count per track
  row; the one hook is off per colour and passes 100 % (`gameplay-map.md`, "Collecting a block").
- **"The score on the HUD next to the record."** The record is a final score, with bonuses; the raw score is 20–50 %
  under it over a good run. Ask which the player means, and say the difference.
- **"Hide the flying coins by muting the particle renderer."** That node pays the score; muted, the player scores
  nothing.

The game's traps that the first big HUD script found the hard way are listed in skill `tweaker-lua-api`, "The game's
own traps". Go through them before a verdict on anything that counts, scores or replaces the HUD: several turn
"can do" into "can do, with caveats", and the caveats belong in the spec.

When you are about to explain the game to a player, remember who knows it better: if you are not sure, say what
you think and ask whether that is how they see it in the game.

## Verdicts

After the research, say which of these it is, and why:

- **Can do.** Reading anything the game computes; reacting when the game does something; switching off pieces of
  the game's own HUD and drawing your own in their place; text, rectangles, lines, gradients, glows and the
  plugin's built-in icons, animated over time; changing values in the game when the change is traced and safe;
  **settings the player changes in the game** — a tab of the script's own in the overlay menu with switches,
  sliders, numbers, choices, colour pickers, buttons and lists; **remembering things between sessions** —
  settings, personal records, a history of runs, in the script's own files.
- **Can do, with caveats** — say each: a value that exists only at certain moments (some statistics are filled
  only at the end of a run); an effect that has to be approximated from rectangles, lines and glows; a change to
  the game's logic (what else it touches, and the leaderboard question below); something that depends on a channel
  index that may differ in another version of the game; a cost per frame that is worth measuring; text in the
  script's menu tab is **Latin only** (no Cyrillic in the overlay's fonts) — a Russian-speaking player gets an
  English tab; a setting changed with the mouse in the overlay, never with a hotkey.
- **Needs a change to the Tweaker** — the current scripting API does not support it; the plugin would have to add
  something. This is a normal answer, not a failure: the API is young and does little yet. See "When the API
  cannot do it" below. The usual cases:
  reading the keyboard or the mouse — hotkeys, clicking on the HUD, steering anything by hand (a script's input
  is its menu tab only); typing text into a field;
  files anywhere but the script's own folder — importing a file the user has, exporting into another program;
  images, textures or particles of its own (not even a picture from the script's own folder), circles and
  arbitrary shapes;
  sound;
  drawing into the 3D scene rather than over it;
  talking to other scripts, or to the Tweaker's skins, playlists and settings;
  adding or removing rows of the game's tables; unhooking one channel from another;
  making the game run one of its own handlers on demand — a script can react to a handler, cancel it, switch it
  off and change values around it, but not call it.
- **Cannot be done now** — and why. A **replay mod** is the usual one: a run can be *recorded* into the script's
  files now, but nothing can play it back — a script cannot steer the car or feed the game the player's input. Say
  what would have to exist first.

Be concrete about the caveats; "it might be hard" helps nobody.

## When the API cannot do it

1. **Say it plainly**: "the current version of the Tweaker's scripting API does not support this", and the one or
   two reasons in the user's terms ("a script can notice when the game runs its undo, but it cannot make the game
   burn matches in the middle of it"). Do not soften it into "tricky".
2. **Offer the useful next step: an issue for the Tweaker's developers** at
   github.com/Unchpokable/Audiosurf-Tweaker (Issues). Offer to draft it — the user posts it:

   ```
   Title:    Scripting API: <what is missing, in a few words>
   What I want to make:   <the player's view, from the spec>
   What the script would need:  <the capability - "call a game handler", "read a hotkey", "draw an image">
   What the current API allows, and why it is not enough:  <what you found: handlers, channels, cgrpy output>
   Game version / Tweaker version:  <if known>
   ```
3. **Only if the user insists** on getting something anyway: build what the current API allows, and say before
   writing it what will be missing, approximate or fragile ("this redraws the board after the undo; matches still
   burn only on the game's own timer"). Never present the approximation as the thing they asked for, and never
   reach outside the API — no game file patches, no DLLs, no other process — to close the gap.

## Talking to someone who knows the engine

A modder who talks in groups, handlers, vtables and offsets does not need the player-level questions: confirm the
spec in their terms, in one round, and get to the research. Disagree with them when the game says otherwise —
with the evidence, as between colleagues.

## The cheating question

A script that only reads and draws is not cheating. A script that **changes the game's logic** — scoring, the
chain, burn timers, what matches, what the road generates, how an ability works — changes the score **the game
itself sends to the leaderboard**, and nothing detects it. That is formally cheating.

- Changes that make the game **harder** (denser traffic, shorter timers, no shoulder): fine, no question.
- Changes that plainly make **scoring easier** (a bigger chain multiplier, longer burn timers, an all-red road,
  an old, stronger Undo, editing the statistics the game sends, "always 99 999 999"): say it is cheating, ask
  whether that is really the goal, and carry on if it is. Once — not in every message.
- Practice tools and personal fun are legitimate; say so if the user seems worried.

Background: `.tweaker/docs/internals/playing-audiosurf.md#mods-and-the-leaderboards`.

## The spec

Short, in the user's language, in player terms first:

```
What you will see:   <one or two sentences, as the player sees it>
When:                <modes, characters, leagues; run / pause / results / menu>
Where:               <screen position; what it replaces or sits next to>
What changes in the game:  nothing - it only reads and draws  |  <exactly what, and the leaderboard note>
Settings:            none  |  a "<label>" tab in the overlay menu: <controls>; remembered between sessions or not
Not included:        <what was asked about and left out, and why>
Verdict:             can do | can do, with: <caveats> | needs a Tweaker change: <what> | cannot now: <why>
How it works (for the record):  <groups, channels, handlers, mutes, writes - found with cgrpy / docs>
How you will check it:  <what to start in the game and what you should see>
Assumptions:         <defaults nobody confirmed>
```

Then: "Shall I build it like this?" — and wait.

Worked examples, including the two requests this skill is named for, are in `references/examples.md`.
