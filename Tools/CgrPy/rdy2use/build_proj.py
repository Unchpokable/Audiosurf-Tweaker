r"""Parse every .cgr under engine/ and cache the result. Run this first; it is the one tool that does its
job with no arguments.

    cgrpy build_proj                       # finds the game via cgr.paths
    cgrpy build_proj "<...>\engine"        # or point it somewhere explicitly

Writes `.cache/proj.pkl`, which every graph tool loads instead of re-parsing ~160 files, and
`.cache/proj.stamp.json`, which lets them warn when the game's files have changed since. Takes a few
seconds; the cache is ~200 MB and stays in this folder. `Cache/` under engine/ - Audiosurf Radio songs the
game downloaded, a different set for every player - is left out.

**Read what it prints - it is a correctness check, not decoration.**

    157 groups, 131255 channels, 0 errors, 4.9s
    unparsed bytes: as expected (15 groups with known losses, 3141952 bytes)

The parse is a framing problem: chunks are self-describing, so if the walker ever loses sync it does not
stop, it carries on producing plausible nonsense. Unparsed bytes are where it skipped. Some groups always
lose a little - the baseline below, measured 2026-10-18 on the Steam release - and that is fine:

* **Asset-heavy groups** (`Environment/Squid_*`, `Environment/SetPieces*`, `XX_StartHere.cgr`, a few menus):
  embedded geometry and textures derail framing at blob boundaries, and the losses land in trailing
  non-channel chunks rather than in the graph. Every group the gameplay notes analyse is clean, except
  `XX_StartHere.cgr`, which loses only trailing blobs.
* `CHCO` (a group's own channel count) does not reconcile for `Intros/XX_OnlineHighScoresTable.cgr` and
  `Render/Render_IndustrialTunnel.cgr`.

A group that loses **more** than its baseline, or a group not in it that loses anything, is reported by
name: treat that as a bug in the parser, or a game file that is not the one the baseline was measured on -
not as a property of the game.
"""

import pickle
import sys
import time
from pathlib import Path

from cgr.cache import write_stamp
from cgr.paths import engine_dir, project_pickle
from cgr.project import Project

# Unparsed bytes per group on the Steam release, measured 2026-10-18 (see the docstring).
BASELINE = {
    "Environment/Squid_Black.cgr": 1313760,
    "Environment/Squid_White.cgr": 1313760,
    "Environment/SetPiecesBlack2.cgr": 107400,
    "Environment/SetPiecesBlack.cgr": 92844,
    "Environment/SetPieces.cgr": 72900,
    "Environment/SetPiecesBlack3.cgr": 72900,
    "Environment/SetPiecesWhite.cgr": 72900,
    "Environment/SetPiecesWhite2.cgr": 72900,
    "XX_StartHere.cgr": 20968,
    "Intros/Ladder.cgr": 740,
    "Intros/XX_OnlineHighScoresTable.cgr": 678,
    "SongSelector/CDBrowser.cgr": 112,
    "Render/Render_IndustrialTunnel.cgr": 38,
    "Intros/CharacterTutorial.cgr": 36,
    "SongSelector/RadioBrowser.cgr": 16,
}


def main(engine=None):
    engine = Path(engine) if engine else engine_dir()

    started = time.time()
    project = Project(str(engine))

    channels = sum(len(g.channels) for g in project.groups.values())
    print(f"{len(project.groups)} groups, {channels} channels, {len(project.errors)} errors, "
          f"{time.time() - started:.1f}s")
    for path, error in project.errors.items():
        print(f"  could not parse {path}: {error}")

    lost = {rel: g.meta.get("bad_bytes", 0) for rel, g in project.groups.items() if g.meta.get("bad_bytes", 0)}
    worse = {rel: n for rel, n in lost.items() if n > BASELINE.get(rel, 0)}
    if worse:
        print("\nWARNING: more unparsed bytes than the baseline - see this file's docstring before trusting "
              "anything derived from these groups:", file=sys.stderr)
        for rel, n in sorted(worse.items(), key=lambda item: -item[1]):
            print(f"  {rel}: {n} bytes (baseline {BASELINE.get(rel, 0)})", file=sys.stderr)
    else:
        print(f"unparsed bytes: as expected ({len(lost)} groups with known losses, {sum(lost.values())} bytes)")

    target = project_pickle()
    with open(target, "wb") as handle:
        pickle.dump(project, handle)
    write_stamp(engine)
    print(f"-> {target}")


if __name__ == "__main__":
    main(*sys.argv[1:])
