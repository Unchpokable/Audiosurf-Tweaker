r"""Trace the draw path of a Quest3D scene node.

The renderable triple is always the same shape (see reversing-journal-geometry.md §4.3):

    3D Object   port0 -> Motion/Matrix     transform
                port2 -> Surface
    Surface     port0 -> <geometry>        3D ObjectData / Primitive / Tune_* / ...
                port1 -> Material
                port2 -> <texture>         Texture / RenderTexture / ChannelSwitch

`chain()` walks that downwards from any node; `callers()` walks upwards to find
what actually invokes it each frame. Both follow cross-group import stubs.

    uv run python render.py chain   <group.cgr> <channel>
    uv run python render.py callers <group.cgr> <channel>
    uv run python render.py texusers <group.cgr> <channel>
"""

import pickle
import sys
from collections import defaultdict

from cgr.project import ext_target

_P = None


def P():
    global _P
    if _P is None:
        from cgr.paths import project_pickle

        _P = pickle.load(open(project_pickle(), "rb"))
    return _P


def label(g, c):
    if c is None:
        return "<none>"
    t = ext_target(c)
    ext = f"  EXT->{t[1]}::{t[0]}" if t else ""
    txt = f" text={c.text!r}" if c.text else ""
    val = ""
    if c.type_name in ("Value", "Expression Value") and c.value is not None:
        val = f" ={c.value:g}"
    return f"#{c.index} {c.type_name} {c.name!r}{txt}{val}{ext}"


def resolve(g, c):
    """Follow an import stub to (group, channel) in the producing group."""
    t = ext_target(c)
    if not t:
        return g, c
    name, gf = t
    for gg in P().by_file.get(gf, []):
        if name in gg.by_name:
            return gg, gg.by_name[name][0]
    return g, c


def chain(rel, idx, depth=0, seen=None, maxd=7, port=None):
    g = P().groups[rel] if isinstance(rel, str) else rel
    # NB: str has an .index method, so duck-typing on it picks the wrong branch
    c = idx if hasattr(idx, "chunks") else g.get(int(idx))
    seen = seen if seen is not None else set()
    pad = "  " * depth
    pfx = f"port{port} -> " if port is not None else ""
    if c is None:
        print(f"{pad}{pfx}<missing>")
        return
    print(f"{pad}{pfx}{label(g, c)}")
    key = (g.rel, c.index)
    if key in seen or depth >= maxd:
        if key in seen:
            print(f"{pad}  ... (shown above)")
        return
    seen.add(key)
    g2, c2 = resolve(g, c)
    if (g2.rel, c2.index) != key:
        print(f"{pad}  => {g2.rel}")
        chain(g2, c2, depth + 1, seen, maxd)
        return
    for p, kids in sorted(g.ports(c).items()):
        for k in kids:
            chain(g, k, depth + 1, seen, maxd, port=p)


def _rev(g):
    r = defaultdict(list)
    for ch in g.channels.values():
        for l in ch.links:
            if l.child >= 0:
                r[l.child].append((ch.index, l.ul))
    return r


def callers(rel, idx, depth=0, seen=None, maxd=8):
    """Walk upwards to whatever drives this node each frame."""
    g = P().groups[rel] if isinstance(rel, str) else rel
    idx = int(idx)
    seen = seen if seen is not None else set()
    key = (g.rel, idx)
    if key in seen or depth >= maxd:
        return
    seen.add(key)
    c = g.get(idx)
    print("  " * depth + label(g, c))
    ps = _rev(g).get(idx, [])
    if not ps:
        # nothing local: maybe another group imports this channel by name
        if c is not None and c.name:
            for gg in P().groups.values():
                if gg.rel == g.rel:
                    continue
                for stub in gg.channels.values():
                    t = ext_target(stub)
                    if t and t[0] == c.name and P().path_stem(t[1]) in (
                            P().path_stem(g.file), g.file):
                        print("  " * (depth + 1) + f"<= imported by {gg.rel}")
                        callers(gg, stub.index, depth + 2, seen, maxd)
        return
    for pi, port in ps:
        print("  " * (depth + 1) + f"<- port{port} of")
        callers(g, pi, depth + 2, seen, maxd)


def texusers(rel, idx):
    """Every Surface that ends up using this texture channel, through switches."""
    g = P().groups[rel]
    rev = _rev(g)
    front = [(int(idx), [])]
    out = []
    seen = set()
    while front:
        i, path = front.pop()
        if i in seen:
            continue
        seen.add(i)
        for pi, port in rev.get(i, []):
            c = g.get(pi)
            if c is None:
                continue
            if c.type_name == "Surface" and port == 2:
                out.append((pi, path))
            elif c.type_name in ("ChannelSwitch", "CallSelected", "Value", "CallOnGetChild"):
                front.append((pi, path + [f"{c.type_name}#{pi}:port{port}"]))
    for pi, path in out:
        print(f"  Surface #{pi}  via {' -> '.join(path) if path else 'direct'}")
        chain(g, pi, depth=2, maxd=3)


def find(rel, name):
    """Channel indices whose name contains `name`. The way to get an index for the commands below.

    Prints every match rather than the first, because **names are not unique**: a group can hold
    dozens of channels called `Set Vector`, and TrafficCommander has two different Values both called
    `TrafficType`. A tool that quietly returned the first would be wrong exactly when it matters.
    """
    g = P().groups[rel]
    needle = name.lower()
    hits = sorted(
        (c for c in g.channels.values() if needle in (c.name or "").lower()),
        key=lambda c: c.index,
    )

    for c in hits:
        print(f"  #{c.index:<6} {c.type_name:<22} {c.name}")

    print(f"{len(hits)} match(es) in {rel}")
    if len(hits) > 1:
        print("NOTE: more than one - pick by index, and say in any note WHY that one.")


COMMANDS = dict(find=find, chain=chain, callers=callers, texusers=texusers)

USAGE = """Trace the draw path of a Quest3D scene node. Needs the cache: run `uvrun build_proj` once.

    uvrun render find     <group.cgr> <name>    # channel indices matching a name
    uvrun render chain    <group.cgr> <index>   # down: Surface -> geometry / material / texture
    uvrun render callers  <group.cgr> <index>   # up: who invokes it each frame, imports included
    uvrun render texusers <group.cgr> <index>   # which Surfaces use this texture

The last three take an **index**, not a name, and `find` is how you get one. That split is
deliberate: channel names are not unique within a group, so resolving one silently would pick a
channel you did not mean - which is a mistake this project has actually made and paid for.

Where it was used: most of reversing-journal-render.md §5. Working out that `GridColor` colours road
furniture and the hit flash rather than the puzzle tiles came from `callers` on it, after `chain`
showed the tiles are drawn down a path whose Surface carries no Material at all."""


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print(USAGE)
        raise SystemExit(2)
    COMMANDS[sys.argv[1]](*sys.argv[2:])
