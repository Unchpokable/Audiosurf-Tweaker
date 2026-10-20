"""The parsed-project cache: building it, loading it, and noticing when it no longer matches the game.

`build_proj` parses every group once and pickles the result; every graph tool loads that instead of
re-parsing. Next to the pickle goes a stamp - size and modification time of every .cgr it was built from -
so that a tool can tell the cache was made from a different install, or before a game update, or before
someone patched a group, and say so instead of answering from stale data.
"""

import json
import pickle
import sys
from pathlib import Path

from cgr.paths import engine_dir, project_pickle, project_stamp


def is_downloaded(rel: str) -> bool:
    """`Cache/...` holds what the game downloaded - Audiosurf Radio songs - not the game's own logic.

    Every player has a different set, so it is left out of the project: otherwise group counts and the
    parse baseline would depend on which radio songs someone happened to play.
    """
    return rel.replace("\\", "/").split("/", 1)[0].lower() == "cache"


def game_files(engine: Path):
    """Every .cgr of the game itself, as (relative path with '/', path)."""
    for path in sorted(engine.rglob("*.cgr")):
        rel = str(path.relative_to(engine)).replace("\\", "/")
        if not is_downloaded(rel):
            yield rel, path


def stamp(engine: Path) -> dict:
    out = {}
    for rel, path in game_files(engine):
        info = path.stat()
        out[rel] = [info.st_size, info.st_mtime_ns]
    return out


def write_stamp(engine: Path):
    project_stamp().write_text(json.dumps({"engine": str(engine), "files": stamp(engine)}), encoding="utf-8")


def load_project(warn=True):
    """The cached project. Exits with instructions when there is none; warns when it is out of date."""
    target = project_pickle()
    if not target.exists():
        sys.exit("No project cache yet. Run `cgrpy build_proj` once (a few seconds), then this again.")

    if warn:
        _check_stamp()

    with open(target, "rb") as handle:
        return pickle.load(handle)


def _check_stamp():
    try:
        recorded = json.loads(project_stamp().read_text(encoding="utf-8"))
    except (OSError, ValueError):
        print("WARNING: the project cache predates freshness checks - run `cgrpy build_proj` to rebuild it.",
              file=sys.stderr)
        return

    try:
        engine = engine_dir()
    except Exception:
        return  # no game to compare with: the cache is all there is, and that is the user's call

    if str(engine) != recorded.get("engine"):
        print(f"WARNING: the project cache was built from {recorded.get('engine')}, but the game is now found at "
              f"{engine}. Run `cgrpy build_proj` if that is a different install.", file=sys.stderr)
        return

    now = stamp(engine)
    then = recorded.get("files", {})
    changed = sorted(rel for rel in set(now) | set(then) if now.get(rel) != then.get(rel))
    if changed:
        shown = ", ".join(changed[:5]) + (f" and {len(changed) - 5} more" if len(changed) > 5 else "")
        print(f"WARNING: {len(changed)} game file(s) changed since the project cache was built ({shown}). "
              "Answers about them may be stale - run `cgrpy build_proj`.", file=sys.stderr)
