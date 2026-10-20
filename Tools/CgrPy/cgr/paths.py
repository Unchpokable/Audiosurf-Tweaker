"""Where the game is, and where this package keeps what it generates.

Every tool here reads files from an Audiosurf install. Those files are not shipped with CgrPy and never
will be - it is someone else's game - so each tool has to find them at run time. One place answers that,
in this order:

1. ``AUDIOSURF_DIR`` in the environment: the Audiosurf folder or its ``engine`` folder, the same rule as
   twllink's. Audiosurf Tweaker sets it for an editor it opens.
2. Audiosurf Tweaker's own setting, when CgrPy runs from the Tweaker's ``DevTools\\CgrPy`` folder: the
   game folder the Tweaker was pointed at, read from ``TweakerUI.exe.config`` two folders up.
3. ``<repo>/Audiosurf_Steam``, when CgrPy runs from the Tweaker's source repository (``Tools/CgrPy``).
4. Steam: the Steam folder from the registry and every library listed in its
   ``steamapps/libraryfolders.vdf``, each checked for ``steamapps/common/Audiosurf``.

A tool that takes a path argument should still accept one; this is the fallback, not a replacement for
saying which file you mean.

Everything CgrPy generates stays inside its own folder (``.cache/``): the parsed-project cache, and - set
by the launchers - uv's package cache and the Python it installs. Nothing goes to the user's profile.
"""

import os
import re
import xml.etree.ElementTree as ElementTree
from pathlib import Path

# <root>/cgr/paths.py: the CgrPy folder is one level up, whatever it is called and wherever it lives.
CGRPY_ROOT = Path(__file__).resolve().parents[1]

# Audiosurf's Steam app id. A folder named after it, or a Steam manifest for it, is how a live install
# is told apart from a copy (is_live_game).
STEAM_APP_ID = "12900"


class GameNotFound(RuntimeError):
    pass


def _engine_ok(root: Path) -> bool:
    return (root / "engine").is_dir()


def _from_env(tried: list):
    value = os.environ.get("AUDIOSURF_DIR")
    if not value:
        return None
    path = Path(value.strip().strip('"'))
    for root in (path, path.parent):
        if _engine_ok(root) and (root == path or path.name.lower() == "engine"):
            return root
    tried.append(f"AUDIOSURF_DIR = {path} (no engine folder there, and it is not one)")
    raise GameNotFound(
        f"AUDIOSURF_DIR is set to {path}, which is neither the Audiosurf folder nor its engine folder.\n"
        "Point it at the folder that contains engine\\, or unset it."
    )


def _tweaker_folder():
    """The Audiosurf Tweaker folder when CgrPy runs from <Tweaker>/DevTools/CgrPy, else None."""
    devtools = CGRPY_ROOT.parent
    if devtools.name.lower() != "devtools":
        return None
    return devtools.parent


def _from_tweaker(tried: list):
    tweaker = _tweaker_folder()
    if tweaker is None:
        return None
    for config in sorted(tweaker.glob("*.exe.config")):
        try:
            settings = ElementTree.parse(config).getroot().find("appSettings")
        except (OSError, ElementTree.ParseError):
            continue
        if settings is None:
            continue
        for entry in settings.findall("add"):
            if entry.get("key") != "TexturesPath":
                continue
            # The Tweaker stores <game>\engine\textures; "None" until the user has picked one.
            textures = Path(entry.get("value") or "")
            root = textures.parent.parent
            if textures.name and _engine_ok(root):
                return root
            tried.append(f"{config.name}: TexturesPath = {entry.get('value')!r} (not a game folder)")
            return None
    tried.append(f"{tweaker}: no TexturesPath in the Tweaker's settings")
    return None


def _from_repo(tried: list):
    repo = CGRPY_ROOT.parents[1]
    if not (repo / "Tools" / "CgrPy").is_dir() or CGRPY_ROOT != repo / "Tools" / "CgrPy":
        return None
    candidate = repo / "Audiosurf_Steam"
    if _engine_ok(candidate):
        return candidate
    tried.append(str(candidate))
    return None


def _steam_folders():
    folders = []
    try:
        import winreg

        for hive, key, name in (
            (winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
            (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam", "InstallPath"),
        ):
            try:
                with winreg.OpenKey(hive, key) as handle:
                    folders.append(Path(winreg.QueryValueEx(handle, name)[0]))
            except OSError:
                pass
    except ImportError:
        pass
    folders += [Path(r"C:\Program Files (x86)\Steam"), Path(r"C:\Program Files\Steam")]

    libraries = []
    for steam in folders:
        libraries.append(steam)
        vdf = steam / "steamapps" / "libraryfolders.vdf"
        try:
            text = vdf.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in re.finditer(r'"path"\s+"([^"]+)"', text):
            libraries.append(Path(match.group(1).replace("\\\\", "\\")))

    unique = []
    for library in libraries:
        if library not in unique:
            unique.append(library)
    return unique


def _from_steam(tried: list):
    for library in _steam_folders():
        candidate = library / "steamapps" / "common" / "Audiosurf"
        if _engine_ok(candidate):
            return candidate
        tried.append(str(candidate))
    return None


def game_dir() -> Path:
    """The Audiosurf install root - the directory holding `engine/`."""
    tried = []
    for source in (_from_env, _from_tweaker, _from_repo, _from_steam):
        found = source(tried)
        if found is not None:
            return found

    listed = "\n  ".join(tried) if tried else "(nothing to look at)"
    raise GameNotFound(
        "Could not find an Audiosurf install. Set AUDIOSURF_DIR to the folder that contains engine\\.\n"
        f"Looked in:\n  {listed}"
    )


def engine_dir() -> Path:
    """`engine/` - HighPoly.dll, QuestViewer.exe, channels.lst, and the .cgr tree."""
    return game_dir() / "engine"


def channels_dir() -> Path:
    """`engine/channels/` - one DLL per channel type, named by its guid."""
    return engine_dir() / "channels"


def cache_dir() -> Path:
    """Where generated things go: inside this folder, never in the user's profile. Safe to delete."""
    path = CGRPY_ROOT / ".cache"
    path.mkdir(exist_ok=True)
    return path


def project_pickle() -> Path:
    """The parsed-project cache `build_proj.py` writes and the graph tools read.

    Roughly 200 MB, because it holds every channel of every group. Never commit or share it: it is derived
    entirely from the game's own files, takes seconds to rebuild, and would otherwise be a copy of someone
    else's game data.
    """
    return cache_dir() / "proj.pkl"


def project_stamp() -> Path:
    """Size and modification time of every .cgr at the moment the cache was built (cgr.cache)."""
    return cache_dir() / "proj.stamp.json"


def is_live_game(path) -> bool:
    """Whether `path` looks like it belongs to the Steam install of the game rather than to a copy.

    A heuristic, on purpose generous: a word `steam`/`steamapps` or the app id 12900 anywhere in the path,
    or a Steam `.vdf`/`.acf` file in any folder above it. A false positive costs one flag (`--not-live`);
    a false negative costs someone their game.
    """
    full = Path(path).resolve()
    for part in full.parts:
        lowered = part.lower()
        if "steam" in lowered or STEAM_APP_ID in lowered:
            return True
    for folder in full.parents:
        try:
            if any(folder.glob("*.vdf")) or any(folder.glob("appmanifest_*.acf")):
                return True
        except OSError:
            continue
    return False


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
