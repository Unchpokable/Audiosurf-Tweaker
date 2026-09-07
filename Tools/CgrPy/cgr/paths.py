"""Where the game is.

Every tool here reads files from an Audiosurf install. Those files are not in the repository and
never will be (`Audiosurf_Steam/` is gitignored - it is someone else's game), so each tool has to
find them at run time.

The scripts this package grew out of each hardcoded an absolute path to one particular machine. That
worked for exactly as long as nobody else ran them, which is the same failure the harness `vcenv.bat`
avoids by asking `vswhere` instead of writing out a Visual Studio path. So: one place, three ways to
answer, and a clear error rather than a confusing one.

Resolution order:

1. ``AUDIOSURF_DIR`` in the environment - set this if your install is elsewhere.
2. ``<repo>/Audiosurf_Steam`` - where a working copy usually keeps it.
3. The Steam default under Program Files, both 32- and 64-bit.

A tool that takes a path argument should still accept one; this is the fallback, not a replacement
for saying which file you mean.
"""

import os
from pathlib import Path

# Tools/CgrPy/cgr/paths.py -> repo root is four levels up.
REPO_ROOT = Path(__file__).resolve().parents[3]

_CANDIDATES = [
    REPO_ROOT / "Audiosurf_Steam",
    Path(r"C:\Program Files (x86)\Steam\steamapps\common\Audiosurf"),
    Path(r"C:\Program Files\Steam\steamapps\common\Audiosurf"),
]


class GameNotFound(RuntimeError):
    pass


def game_dir() -> Path:
    """The Audiosurf install root - the directory holding `engine/`."""
    override = os.environ.get("AUDIOSURF_DIR")
    if override:
        path = Path(override)
        if not (path / "engine").is_dir():
            raise GameNotFound(f"AUDIOSURF_DIR is set to {path}, but there is no engine/ directory in it")
        return path

    for candidate in _CANDIDATES:
        if (candidate / "engine").is_dir():
            return candidate

    tried = "\n  ".join(str(c) for c in _CANDIDATES)
    raise GameNotFound(
        "Could not find an Audiosurf install. Set AUDIOSURF_DIR to the folder that contains engine/.\n"
        f"Looked in:\n  {tried}"
    )


def engine_dir() -> Path:
    """`engine/` - HighPoly.dll, QuestViewer.exe, channels.lst, and the .cgr tree."""
    return game_dir() / "engine"


def channels_dir() -> Path:
    """`engine/channels/` - one DLL per channel type, named by its guid."""
    return engine_dir() / "channels"


def cache_dir() -> Path:
    """Where generated things go. Gitignored, and safe to delete - everything in it is rebuildable."""
    path = Path(__file__).resolve().parents[1] / ".cache"
    path.mkdir(exist_ok=True)
    return path


def project_pickle() -> Path:
    """The parsed-project cache `build_proj.py` writes and the graph tools read.

    Roughly 200 MB, because it holds every channel of all 161 groups. Never commit it: it is derived
    entirely from the game's own files, takes under a minute to rebuild, and would otherwise put a
    copy of someone else's game data in the repository.
    """
    return cache_dir() / "proj.pkl"


def channel_type_dll(name: str) -> Path:
    """The DLL implementing a channel type, by the type's display name.

    Saves the two-step of running chtypes.py and pasting a guid. Raises if the name is not in
    channels.lst, because a typo here otherwise surfaces as "file not found" on a guid you did not
    recognise anyway.
    """
    from cgr.chtypes import type_to_dll

    table = type_to_dll()
    if name not in table:
        near = [k for k in table if name.lower() in k.lower()]
        hint = f" Did you mean: {', '.join(sorted(near)[:5])}?" if near else ""
        raise KeyError(f"no channel type called {name!r} in channels.lst.{hint}")

    return channels_dir() / table[name]
