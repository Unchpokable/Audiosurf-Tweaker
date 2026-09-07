import pickle, collections
from cgr.project import *
from cgr.decomp import Decompiler
P = pickle.load(open("proj.pkl","rb"))
sc = P.groups["Scores/StatCollector.cgr"]
D  = Decompiler(P, sc, max_depth=4)

# the run-stat vector = targets of the master reset Set Value #11
reset = sc.get(11)
stats = [sc.get(i) for i in sc.ports(reset)[1]]

# who writes each? scan every Set Value in StatCollector (port1 = targets)
writers = collections.defaultdict(list)
for c in sc.channels.values():
    if c.type_name not in ("Set Value","Set Text"): continue
    p = sc.ports(c)
    for t in p.get(1, []):
        src = p.get(0, [])
        rhs = D.value(src[0]) if src else str(D.__class__ and __import__('struct').unpack('<f', c.tag('FLVA'))[0] if c.tag('FLVA') else '?')
        writers[t].append((c.index, rhs))

# reverse parent map for context
par = collections.defaultdict(list)
for c in sc.channels.values():
    for l in c.links:
        if l.child>=0: par[l.child].append(c.index)
def ctx(i, hops=4):
    seen=set(); cur=i
    for _ in range(hops):
        ps=[x for x in par.get(cur,[]) if x not in seen]
        if not ps: break
        cur=ps[0]; seen.add(cur)
        c=sc.get(cur)
        if c and c.name and c.name.startswith("Do_"): return c.name
    return ""

with open("out/stat_writers.txt","w",encoding="utf-8") as f:
    f.write("RUN STAT VECTOR (targets of master reset Set Value #11)\n"+"="*76+"\n")
    for s in stats:
        f.write(f"\n{s.name}  (#{s.index}, {s.type_name})\n")
        for wi, rhs in writers.get(s.index, []):
            f.write(f"    <- #{wi:5d} {ctx(wi):34s} := {rhs[:110]}\n")
print(open("out/stat_writers.txt",encoding="utf-8").read())
