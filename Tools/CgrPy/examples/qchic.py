"""CHIC (ingoreTreeCountState_, +0x60) per channel - the flag graph.py discards.

    uv run python qchic.py

graph.py lists CHIC in HEADER and drops it, so `channel.chunks` never contains it and
any test written against `chunks` silently answers "absent" for every channel in the
project. This re-walks the raw chunk stream instead.

CHIC = 1 means "do not memoise" - the channel recomputes on every read within a frame,
which is what lets the game's own ForLoop walk an Array Value column. CHIC = 0 (or no
chunk) means it IS memoised, and a second read in the same frame returns the first
read's value unless the memo is invalidated by hand (lua_channels::read_array).
"""
import collections
import struct
import sys

from cgr.core import load, load_channel_types
from cgr.graph import iter_chunks2
from cgr.project import Project

from cgr.paths import engine_dir

ENGINE = sys.argv[1] if len(sys.argv) > 1 else str(engine_dir())


def chic_map(path):
    """{channel_index: chic_value_or_None} for one .cgr, straight off the chunk stream."""
    out = {}
    cur = None
    for t, off, ln, d in iter_chunks2(load(path)):
        if t == "CHIX":
            cur = struct.unpack("<i", d)[0]
            out.setdefault(cur, None)
        elif t == "CHIC" and cur is not None and len(d) >= 1:
            out[cur] = d[0]
    return out


types = load_channel_types(ENGINE + r"\channels.lst")
P = Project(ENGINE, files=[ENGINE + r"\sounds\VisMusic.cgr"])
vm = P.groups["sounds/VisMusic.cgr"]
m = chic_map(ENGINE + r"\sounds\VisMusic.cgr")

print("=== the two spectrum columns in VisMusic ===")
for name in ("New Table: SpectrumHit", "SpectrumHit256", "SpectrumHit_50"):
    for c in vm.by_name.get(name, []):
        v = m.get(c.index)
        state = "not memoised" if v == 1 else "MEMOISED"
        print(f"  #{c.index:<5d} {name:<24s} CHIC={v}  -> {state}")

print()
print("=== every Array Value in VisMusic, for context ===")
hist = collections.Counter()
for c in vm.channels.values():
    if c.type_name == "Array Value":
        hist[m.get(c.index)] += 1
for v, n in sorted(hist.items(), key=lambda kv: (kv[0] is None, kv[0])):
    print(f"  CHIC={v}: {n}")

print()
print("=== project-wide, Array Value only (the 400-of-431 claim in the journal) ===")
whole = Project(ENGINE)
total = collections.Counter()
for rel, g in whole.groups.items():
    mm = chic_map(g.path)
    for c in g.channels.values():
        if c.type_name == "Array Value":
            total[mm.get(c.index)] += 1
n = sum(total.values())
for v, k in sorted(total.items(), key=lambda kv: (kv[0] is None, kv[0])):
    print(f"  CHIC={v}: {k} of {n}")
