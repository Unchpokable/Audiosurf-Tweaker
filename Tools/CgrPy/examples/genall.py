"""Generate the full analysis artifact set for StatCollector & friends."""
import pickle, csv, collections, struct
from cgr.project import *
from cgr.decomp import Decompiler
from cgr.core import latin1

P = pickle.load(open("proj.pkl", "rb"))
import os
OUT = "out/"
os.makedirs(OUT, exist_ok=True)

def api_of(g):
    api = collections.defaultdict(list)
    for gg in P.groups.values():
        for c, name, gf, hit in P.imports(gg):
            if hit and hit[0] is g:
                api[name].append(gg.rel)
    return api

# ---------------------------------------------------------------- 1. API csv
with open(OUT + "public_api.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["group", "exported_name", "channel_type", "index", "consumers", "consumer_groups"])
    for rel in ["Scores/StatCollector.cgr", "Actors/SpecialPurpose.cgr", "Environment/Puzzle.cgr",
                "Actors/Player.cgr", "Environment/Highway.cgr", "Support/Achievements.cgr",
                "Actors/TrafficCommander.cgr", "Support/PlayerLevel.cgr"]:
        g = P.groups[rel]
        for name, cons in sorted(api_of(g).items()):
            t = g.by_name[name][0]
            w.writerow([rel, name, t.type_name, t.index, len(cons), ";".join(sorted(set(cons)))])

# ---------------------------------------------------------------- 2. params
with open(OUT + "group_params.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["group", "param_ordinal", "param_name", "channel_type", "index"])
    for rel, g in sorted(P.groups.items()):
        for i, c in enumerate(group_params(g)):
            w.writerow([rel, i, c.name, c.type_name, c.index])

# ---------------------------------------------------------------- 3. edges
with open(OUT + "crossgroup_edges.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["consumer_group", "producer", "link_count"])
    for (a, b), n in sorted(P.import_edges().items(), key=lambda x: -x[1]):
        w.writerow([a, b, n])

# ---------------------------------------------------------------- 4. tables
with open(OUT + "tables.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["group", "channel", "index", "table_name", "column_ordinal", "column_name"])
    for rel, g in sorted(P.groups.items()):
        for c in sorted(g.channels.values(), key=lambda x: x.index):
            if c.type_name != "Array Table":
                continue
            tn, pend, k = None, None, 0
            for t, d in c.chunks:
                if t == "ATTN":
                    tn = latin1(d)
                elif t == "ATCN":
                    pend = latin1(d)
                elif t == "ATUI" and pend is not None:
                    w.writerow([rel, c.name, c.index, tn, k, pend]); k += 1; pend = None

# ---------------------------------------------------------------- 5. characters
sp = P.groups["Actors/SpecialPurpose.cgr"]
D = Decompiler(P, sp, max_depth=6)
with open(OUT + "characters.txt", "w", encoding="utf-8") as f:
    for j, k in enumerate(sp.ports(sp.get(4))[2]):
        c = sp.get(k)
        f.write(f"\n{'='*72}\n## PlayerVehicleType == {j}  ->  {c.name}  (#{k})\n{'='*72}\n")
        f.write("\n".join(D.action(k)) + "\n")

# ---------------------------------------------------------------- 6. StatCollector logic
sc = P.groups["Scores/StatCollector.cgr"]
Ds = Decompiler(P, sc, max_depth=9)
entries = [n for n in sorted(sc.by_name) if n.startswith("Do")]
with open(OUT + "statcollector_logic.txt", "w", encoding="utf-8") as f:
    for n in entries:
        c = sc.by_name[n][0]
        f.write(f"\n{'='*72}\n### {n}  (#{c.index})\n{'='*72}\n")
        f.write("\n".join(Ds.action(c.index)) + "\n")

# ---------------------------------------------------------------- 7. puzzle
pz = P.groups["Environment/Puzzle.cgr"]
Dp = Decompiler(P, pz, max_depth=7)
with open(OUT + "puzzle_logic.txt", "w", encoding="utf-8") as f:
    for n in [x for x in sorted(pz.by_name) if x.startswith("Do")]:
        c = pz.by_name[n][0]
        f.write(f"\n{'='*72}\n### {n}  (#{c.index})\n{'='*72}\n")
        f.write("\n".join(Dp.action(c.index)) + "\n")

# ---------------------------------------------------------------- 8. lua bindings
with open(OUT + "lua_bindings.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["group", "lua_channel", "index", "GetChild", "bound_channel", "bound_type"])
    for rel, g in sorted(P.groups.items()):
        for c in sorted(g.channels.values(), key=lambda x: x.index):
            if not any(t == "LUSC" for t, _ in c.chunks):
                continue
            for i, k in enumerate(g.ports(c).get(0, [])):
                kc = g.get(k)
                w.writerow([rel, c.name, c.index, i, kc.name if kc else "?", kc.type_name if kc else "?"])
print("done")
