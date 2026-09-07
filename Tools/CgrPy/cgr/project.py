"""Whole-project view: every .cgr under engine/, with cross-group links resolved.

Link model (recovered from the byte layout, see graph.py):

  CHUL = input PORT index on the channel   (which socket)
  CHRP = slot index within that port       (order inside the socket)
  CHLI = child channel index, -1 = empty editor placeholder

Port conventions per channel type:
  Set Value          port0 = source value (absent -> use own FLVA constant)
                     port1 = target channels to write
  If                 port0 = condition, port1 = body
  IfElse             port0 = condition, port1 = then, port2 = else
  ForLoop            port0 = count, port1 = index-out, port2 = body
  Expression Value   port0, slots = operands A, B, C, ... ; FLVA = formula text
  ChannelCaller      port0, slots = call order
  ValueOperator      port0, slots = operands

Cross-group: a channel carrying the bare CHES marker is an *import stub*; its
length-framed CHES record holds `name[80] + sourceGroupFile[80] + authorPath`.
Resolution is by channel NAME inside the named group.
"""

import struct
from collections import defaultdict
from pathlib import Path

from .core import load, load_channel_types, latin1
from .graph import parse_graph


def ext_target(chan):
    """(channel_name, source_group_file) for an import stub, else None."""
    d = chan.ext_record
    if not d or len(d) < 160:
        return None
    name = latin1(d[0:80])
    grp = latin1(d[80:160])
    if not name:
        return None
    return name, grp


class Group:
    def __init__(self, path, types, root):
        self.path = Path(path)
        self.rel = str(self.path.relative_to(root)).replace("\\", "/")
        self.file = self.path.name
        blob = load(self.path)
        self.channels, self.meta = parse_graph(blob, types, group=self.file)
        self.guid = self.meta.get("group_guid")
        self.by_name = defaultdict(list)
        for c in self.channels.values():
            if c.name:
                self.by_name[c.name].append(c)

    def __repr__(self):
        return f"<Group {self.rel} {len(self.channels)}ch>"

    def get(self, i):
        return self.channels.get(i)

    def ports(self, chan):
        """-> {port_index: [child_index, ...]} in slot order, placeholders dropped."""
        out = defaultdict(list)
        for l in chan.links:
            if l.child >= 0:
                out[l.ul].append((l.rp, l.child))
        return {p: [c for _, c in sorted(v)] for p, v in sorted(out.items())}


class Project:
    def __init__(self, engine_dir, files=None):
        self.root = Path(engine_dir)
        self.types = load_channel_types(self.root / "channels.lst")
        paths = files if files else sorted(self.root.rglob("*.cgr"))
        self.groups = {}
        self.errors = {}
        for p in paths:
            p = Path(p)
            try:
                g = Group(p, self.types, self.root)
                self.groups[g.rel] = g
            except Exception as e:  # a few Cache/Web files are truncated downloads
                self.errors[str(p)] = repr(e)
        # A CHES record names the producer either by file ("Highway.cgr") or by
        # bare group name ("Achievements", "StatCollector"); "StartGroup" is the
        # engine's alias for the root project group.
        self.by_file = defaultdict(list)
        for g in self.groups.values():
            self.by_file[g.file].append(g)
            self.by_file[self.path_stem(g.file)].append(g)
        root = self.groups.get("XX_StartHere.cgr")
        if root:
            self.by_file["StartGroup"].append(root)

    @staticmethod
    def path_stem(f):
        return f[:-4] if f.lower().endswith(".cgr") else f

    # ---------------------------------------------------------------- imports
    def imports(self, group):
        """[(stub_channel, target_name, target_group_file, resolved_channel|None)]"""
        out = []
        for c in sorted(group.channels.values(), key=lambda c: c.index):
            t = ext_target(c)
            if not t:
                continue
            name, gf = t
            hit = None
            for g in self.by_file.get(gf, []):
                if name in g.by_name:
                    hit = (g, g.by_name[name][0])
                    break
            out.append((c, name, gf, hit))
        return out

    def import_edges(self):
        """Group-level dependency edges: (consumer_rel, producer_file, count)."""
        edges = defaultdict(int)
        for g in self.groups.values():
            for c, name, gf, hit in self.imports(g):
                edges[(g.rel, gf)] += 1
        return edges


def group_params(group):
    """Declared external INPUTS of a group, in parameter order.

    A channel carrying the bare CHES marker *without* a source record is an
    input parameter of the group. Callers bind actuals through extra ports on
    the import stub: port N binds parameter N, ordered by channel index.
    Verified on StatCollector: #92 ReportMatch_TotalMatchedBlocks -> port 0,
    #96 ReportCarCollected_Color -> 1, #154 UpdatePoints_Delta -> 2,
    #155 UpdatePoints_Color -> 3.
    """
    return [c for c in sorted(group.channels.values(), key=lambda c: c.index)
            if c.external and not c.ext_record]
