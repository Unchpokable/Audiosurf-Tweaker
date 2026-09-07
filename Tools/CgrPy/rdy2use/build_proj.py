r"""Parse every .cgr under engine/ and cache the result. Run this first.

    uvrun build_proj                       # finds the game via cgr.paths
    uvrun build_proj "<...>\engine"        # or point it somewhere explicitly

Writes `.cache/proj.pkl`, which every graph tool here loads instead of re-parsing 161 files. Takes
well under a minute; the cache is ~200 MB and is gitignored.

**Read the numbers it prints - they are the correctness check, not decoration.**

    161 groups, 131356 channels, 0 unparsed bytes, 0 errors

`unparsed bytes` is the one that matters. The container walk is a framing problem: chunks are
self-describing, so if the walker ever loses sync it does not stop, it carries on producing plausible
nonsense. A nonzero count means some region was skipped, and any answer derived from that group is
suspect.

A clean parse is not the same as a complete one. The baseline, measured 2026-08-29:

* `CHCO` (the group's own channel count) reconciles for **159/161** groups. Count records, not
  distinct indices - `CHCO` includes the editor's trailing `CHIX = -1` placeholders, 631 of them in
  `PlayerCar_EraserElite.cgr`. Known misses: `Intros/XX_OnlineHighScoresTable.cgr` and
  `Render/Render_IndustrialTunnel.cgr`.
* Zero unparsed bytes for **144/161**. The other 17 are asset-heavy (`Environment/SetPieces*`,
  `Squid_*`, `XX_StartHere.cgr`) - embedded geometry and textures derail framing at blob boundaries,
  and the losses land in trailing non-channel chunks rather than in the graph.

Every group the gameplay journal analyses is clean on both counts, except `XX_StartHere.cgr`, which
loses only trailing blobs while `CHCO` still reconciles. Treat a regression in either number as a
bug in the parser, not as a property of the game.
"""

import pickle
import sys
import time

from cgr.paths import engine_dir, project_pickle
from cgr.project import Project


def main(engine=None):
    engine = engine or engine_dir()

    started = time.time()
    project = Project(str(engine))

    channels = sum(len(g.channels) for g in project.groups.values())
    bad = sum(g.meta.get("bad_bytes", 0) for g in project.groups.values())

    print(
        f"{len(project.groups)} groups, {channels} channels, "
        f"{bad} unparsed bytes, {len(project.errors)} errors, {time.time() - started:.1f}s"
    )

    target = project_pickle()
    with open(target, "wb") as handle:
        pickle.dump(project, handle)
    print(f"-> {target}")

    if bad:
        print("\nWARNING: nonzero unparsed bytes. See this file's docstring before trusting results.", file=sys.stderr)


if __name__ == "__main__":
    main(*sys.argv[1:])
