# Worked examples

How a grilling goes, on three requests of the kind this skill exists for. The conversation itself happens in the
user's language; these are written in English. What is "found" below is where to look and what to ask the graph —
run the queries yourself, on the user's game, before relying on any of it.

---

## 1. "I want a beautiful HUD, with fireworks"

*(Often in Russian: «хочу красивый худ, и чтоб фейерверки».)*

Almost everything is open: what the HUD shows, where, and what "fireworks" means. Do not design a HUD and ask for
approval — find out what they are missing first.

**Retell:** "A HUD of your own over the game during a run — and some kind of fireworks. Let me ask a few things
so I build what you have in mind."

**Round 1** (each with a default):

1. What should it show that the game's HUD does not, or show better? *Default: the score, the chain and the
   song's progress, styled your way.*
2. Instead of the game's own HUD pieces, or next to them? *Default: instead of the score strip at the top.*
3. Which characters do you play? *Default: all Puzzle characters.*
4. "Fireworks" — the bursts the game already shows when you collect a block (they can be recoloured or switched
   off), or something new drawn on screen, for example when you clear a big match? *Default: ask — they are
   different work.*
5. Any look you like — the Tweaker's overlay, the game's own, something else (a screenshot helps)? *Default: the
   overlay's colours and fonts.*
6. Do you want to tweak it in the game — switch it on and off, change size or colours? *Default: a small tab of its
   own in the overlay menu with those, remembered between sessions.*

**Research before the verdict:** the game's HUD pieces and which can be switched off safely —
`.tweaker/docs/internals/playing-audiosurf.md` ("The screen during a run") and the HUD section of
`.tweaker/docs/internals/gameplay-map.md`; the fireworks are `Debris` (`cgrpy query find "Do_Impact" --group
Debris`), and the shipped `particles.lua` shows how a script changes their tint.

**Verdict, typically:** *can do* for the HUD; for fireworks, *can do* if they mean the game's own (recolour,
retime, switch off), *can do with caveats* if they mean new effects — drawn from rectangles, lines and glows over
the game, no particle images of the script's own (that would need a Tweaker change). The settings tab: *can do*,
with its text in English.

---

## 2. "Make Eraser Elite's undo not trim the saved blocks to the free cells, but push with overflow and burn the matches right away"

*(The jargon request. Every word means something precise to a Puzzle player.)*

**Decode it with the reference first** (`.tweaker/docs/internals/playing-audiosurf.md`, "Eraser in detail"): Eraser's
RMB is *undo* — blocks come back from Eraser's memory onto the board, top down, each into the column it was erased
from. Today a block whose column is full is simply lost, and the memory is cleared. Old versions of the game did it
differently: lay out what fits, burn the matches that makes, lay out the rest, and repeat until the memory is
empty. The user wants the old behaviour. One more thing the reference says: memory today holds at most one board,
and the old trick hoarded several boards of red — so "the old behaviour, fully" also means a larger memory.

**Retell:** "When you undo as Eraser Elite, instead of losing what does not fit, the blocks should go back in
waves: what fits, then the matches burn, then the rest — like in old Audiosurf. Right?"

**Round 1:**

1. Only Eraser Elite, or Eraser (Pro) too? *Default: both, if they share the same undo — to be checked.*
2. Should the matches burn at once, or with the usual burn timer between the waves? *Default: the usual timer.*
3. Do those matches count for score and the chain like any other match? *Default: yes.*
4. If a wave still does not fit (the board is full of non-matching blocks) — overfill, or keep the rest in
   memory? *Default: overfill, as the game does for a column that overflows.*
5. Memory as it is today (one board at most), or as large as in the old versions? *Default: as today — a larger
   memory is a bigger change.*
6. This makes Eraser stronger, so runs with it would score more than the game allows today — and the game
   sends that score to the leaderboard. Is this for practice or personal play? *(The cheating question: ask once,
   continue on yes.)*

**Research:** the undo handler — `cgrpy query dec EraserEffect Do_FireUndo` — and where blocks are dropped;
Eraser's memory (table `EraserUndo`, column `Clipboard`, filled by `Do_AddToClipboard`); how the board is stored (`Puzzle`, the buffer) and how a
collection pass runs (`Puzzle::FindMatches`, `MatchCollectionTicks` — `.tweaker/docs/internals/gameplay-map.md`,
"The collection pass and the chain").

**Verdict — after the research, not before — and most likely *needs a Tweaker change*.** What decides it: a script
cannot make the game run its collection pass, or any other handler, on demand. "Lay out a wave, burn it, lay out
the next" needs exactly that, in the middle of the game's own undo. So the expected answer is: *the current
scripting API does not support this* — the reason in player terms — and *shall I help you write an issue for the
Tweaker's developers?* (the template in `SKILL.md`, "When the API cannot do it").

Only if the user insists on something now: look for what the current API allows — for instance, whether the
game's own collection pass, on its next tick, picks up blocks a script wrote into the board (existing rows only) —
and say up front what that approximation would not do: waves that wait for the game's timer, a chain and score
that may not count them as the game would, a memory still one board deep. Then the cheating note, if not said yet.

---

## 3. "A minimap with animations"

**Retell:** "The little track profile in the top left corner — you want it redrawn your way, with things moving
on it?"

**Round 1:**

1. The game's minimap (the side view of the track with a dot for you), replaced — or a new one somewhere else?
   *Default: replace the game's, same place.*
2. What should move? The dot, the part of the track already played, upcoming intense parts, something else?
   *Default: the dot glides smoothly and the played part fills in.*
3. Bigger than the game's, or the same size? *Default: same size.*
4. Should it look like the game's or like the overlay? *Default: the overlay's colours.*

**Research:** the game's minimap and what draws it (`.tweaker/docs/internals/playing-audiosurf.md` lists
`Do_OverheadMap` and its parts); then the real question — **where the track's shape comes from**. A script can
draw a profile only from numbers it can read: look for the track's heights or positions in a table
(`cgrpy query find "Minimap|Overhead|Height" --group XX_gui`, then the writers of what you find). Note the overlay's
notification feed sits top left by default — the place has to be found at run time.

**Verdict:** *can do* if the track's shape is readable as numbers (the profile is then line segments, the dot a
rectangle or glow, the motion eased); *can do with caveats* if only part of it is; *needs a Tweaker change* if the
shape exists only inside a model or texture the script cannot read. Say which, with the evidence.
