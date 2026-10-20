# Questions to choose from

Pick what the request leaves open. Each question carries a default you propose; the user confirms or corrects.
Terms and where things live: `.tweaker/docs/internals/playing-audiosurf.md`.

## Who and when

- **Which mode?** Puzzle characters (Pointman, Eraser, Pusher, Vegas, Double Vision) and Mono play differently.
  **Both have the board, matches and the chain.** In Puzzle the colours have to be sorted into matches; in Mono
  every coloured block matches every other and the grey ones are obstacles, and Mono also pays for each coloured
  block the moment it is collected (more for each one in a row, back to the minimum on a grey). *Default: whichever
  the request implies; ask if it implies none.*
- **Which characters, which leagues?** A character comes in Casual / Pro / Elite and each is its own entry in the
  game (Eraser is Pro, Eraser Elite is Elite; Pointman Pro is the game's plain "Pointman"). *Default: all of the
  mode's characters.*
- **Ironmode?** Changes overfill (it loses the run) and Mono's abilities. *Default: works with and without.*
- **Double Vision?** Two cars (keyboard and mouse), one board four columns wide, two scores on screen.
  *Default: not specially handled — say so.*
- **When should it show?** Menu, song selector, preparation screen, during the run, while paused, on the results
  screen. *Default: during the run only.*
- **Restart, pause, end of song**: reset, freeze, keep the final value? *Default: resets on restart, freezes on
  pause, stays on the results screen only if the user asks.*
- **Freeride?** No score, can steer itself. *Default: hidden in Freeride.*

## Where and how it looks

- **Where on the screen?** The game's HUD: score top centre with "+N" either side, song progress under it, medals
  top right, minimap top left, chain bar above the song title at the bottom centre. The Tweaker's overlay by
  default: notification feed top left, watermark top right, pins on a side — all movable by the user.
  *Default: a free corner found at run time, never fixed coordinates over something.*
- **Replace a piece of the game's HUD, or add next to it?** Replacing means switching the game's piece off for as
  long as the script runs. *Default: add, unless they say "instead of".*
- **Size, and how it should feel** — "clean", "flashy", "like the game", "like the overlay"? *Default: the
  overlay's theme colours and fonts, compact.*
- **Background theme.** The game has black, grey and white themes, and the sky can be anything a skybox pack makes
  it; text that reads on one can vanish on another. *Default: readable on all of them — a soft dark backdrop behind
  text.*
- **Colours.** The blocks' colours (the game's live palette — the user's custom colours included), the overlay's
  theme, or fixed ones? *Default: block colours for anything about blocks, the theme for the rest.*
- **Animation.** What moves, when, how fast. "Fireworks" — see below. *Default: short eased transitions, nothing
  looping.*

## What it shows or does

- **Which number exactly?** Score, score with the grid bonus, per-colour counts, chain, time to the chain being
  judged, medals, Skill Rating, personal best, the gap to it, blocks left… Many look alike and differ in when the
  game updates them. *Default: what the game itself shows under that name.*
- **Raw score or final score?** The score on screen is raw; the record, the medals at the end and Skill Rating are
  the final score with bonuses (Clean Finish alone is +25 %). A gap to the record only means something final against
  final. *Default: the projected final score wherever it sits next to the record — say so.*
- **Per-colour counts: of what?** Blocks collected out of the blocks the track has, with whites placed — not the
  game's early count, which is wrong until the results screen. *Default: per track block, as the game will count it
  at the end.*
- **Precision and format.** Rounded, with thousands spaced, as a bar, as a difference? *Default: the game's own
  format.*
- **"Fireworks", "effects".** The game already bursts fireworks on every collected block (they can be recoloured
  or switched off). A script can only draw flat shapes over the game — rectangles, lines, glows, gradients, the
  plugin's icons — no particle images of its own. So: change the game's own, or approximate new ones with shapes?
  *Default: ask — the two are different work.*
- **Gameplay changes**: exactly what changes, for which characters, at what moment, and how it interacts with the
  chain and scoring. *Default: none — ask.* Then the cheating question if it applies.

## Settings and memory

- **Anything to adjust in the game?** Show/hide, size, corner, colours, which numbers — as a tab of the script's own
  in the overlay menu (**Insert**, after the built-in tabs): switches, sliders, choices, colour pickers, buttons,
  lists. No hotkeys, no typing; the tab's text is in English (no Cyrillic in the overlay's fonts). *Default: a small
  tab with "Show" and "Size" for anything that draws; nothing for a script that only changes the game.*
- **Remember between sessions?** The settings, a personal record of the script's own, a history of runs — kept in
  the script's own files. *Default: settings yes, everything else only if asked.*

## Delivery

- **For themselves, or to share?** Sharing raises the bar on cost, on other users' setups (other scripts, overlay
  positions, themes) and on the name. *Default: for themselves.*
- **Name** of the script as the Scripts tab shows it. *Default: from the request, in English.*
- **How heavy may it be?** Most HUDs cost well under 0.1 ms a frame; anything that walks big tables every frame
  does not. *Default: light enough not to be noticed.*
