"""Render a Quest3D channel subtree as readable pseudocode.

Two rendering contexts:
  * value context  -> an expression string
  * action context -> statement lines

Node semantics were recovered empirically (see project.py docstring); the
type names come from channels.lst so the renderer keys off those directly.
"""

import struct

from .core import guid, latin1

GENERIC = {"Value", "Set Value", "ChannelCaller", "Expression Value", "If",
           "IfElse", "ForLoop", "Text", "Value Vector", "Set Vector",
           "Array Value", "Array Vector", "ValueOperator", "OneTime", ""}


def f32(d):
    return struct.unpack("<f", d)[0] if d and len(d) == 4 else None


def fmt_num(v):
    if v is None:
        return "?"
    if v == int(v) and abs(v) < 1e9:
        return str(int(v))
    return f"{v:g}"


class Decompiler:
    def __init__(self, project, group, max_depth=6):
        self.P = project
        self.g = group
        self.max_depth = max_depth
        self.tables = self._tables()

    # ------------------------------------------------------------ tables
    def _tables(self):
        """{table_guid: (name, {column_guid: column_name})} across the project."""
        out = {}
        for g in self.P.groups.values():
            for c in g.channels.values():
                if c.type_name != "Array Table":
                    continue
                tg = tn = None
                cols = {}
                pend = None
                for t, d in c.chunks:
                    if t == "ATTG" and len(d) == 16:
                        tg = guid(d)
                    elif t == "ATTN":
                        tn = latin1(d)
                    elif t == "ATCN":
                        pend = latin1(d)
                    elif t == "ATUI" and len(d) == 16 and pend is not None:
                        cols[guid(d)] = pend
                        pend = None
                if tg:
                    old = out.get(tg)
                    if old:
                        old[1].update(cols)
                    else:
                        out[tg] = (tn or "?", cols)
        return out

    def array_ref(self, c):
        v = {t: d for t, d in c.chunks}
        tg = guid(v["TIST"]) if "TIST" in v and len(v["TIST"]) == 16 else None
        cg = guid(v["TISC"]) if "TISC" in v and len(v["TISC"]) == 16 else None
        tn, cols = self.tables.get(tg, ("?table", {}))
        return tn, cols.get(cg, "?col")

    # ------------------------------------------------------------ helpers
    def label(self, c):
        if c is None:
            return "<none>"
        if c.name and c.name not in GENERIC and c.name != c.type_name:
            return c.name
        return f"{c.type_name}#{c.index}"

    def ext(self, c):
        from .project import ext_target
        t = ext_target(c)
        if t:
            return f"{self.P.path_stem(t[1])}::{t[0]}"
        if c.external:
            return f"EXTERN::{c.name}" if c.name else f"EXTERN#{c.index}"
        return None

    def ports(self, c):
        return self.g.ports(c)

    def bound_args(self, c, depth=0, seen=None):
        """Render actuals bound to a remote group's parameters.

        On an import stub, port N carries the value bound to parameter N of the
        target group (params = its bare-CHES-without-record channels, ordered by
        channel index). See project.group_params.
        """
        from .project import ext_target, group_params
        p = self.ports(c)
        if not p:
            return ""
        t = ext_target(c)
        names = {}
        if t:
            for g in self.P.by_file.get(t[1], []):
                if t[0] in g.by_name:
                    names = {i: pc.name for i, pc in enumerate(group_params(g))}
                    break
        parts = []
        for port, kids in sorted(p.items()):
            for k in kids:
                parts.append(f"{names.get(port, f'arg{port}')}={self.value(k, depth + 1, seen or set())}")
        return "(" + ", ".join(parts) + ")"

    # ------------------------------------------------------------ values
    def value(self, idx, depth=0, seen=None):
        seen = seen or set()
        c = self.g.get(idx)
        if c is None:
            return f"<#{idx}>"
        e = self.ext(c)
        if e:
            return e
        if idx in seen or depth > self.max_depth:
            return self.label(c)
        seen = seen | {idx}
        p = self.ports(c)
        t = c.type_name

        if t == "Expression Value":
            expr = latin1(c.tag("FLVA") or b"")
            args = p.get(0, [])
            out = expr
            for i, a in enumerate(args):
                sub = self.value(a, depth + 1, seen)
                out = _sub_operand(out, chr(ord("A") + i), sub)
            return f"({out})"

        if t in ("Value", "Text"):
            kids = p.get(0, [])
            if kids:
                inner = self.value(kids[0], depth + 1, seen)
                return f"{c.name}={inner}" if c.name and c.name not in GENERIC else inner
            # A named channel is a variable; its stored FLVA/STVA is only the
            # last runtime value the editor happened to save, not a constant.
            if c.name and c.name not in GENERIC:
                return c.name
            if t == "Text":
                return f'"{c.text or ""}"'
            return fmt_num(f32(c.tag("FLVA")))

        if t in ("Array Value", "Array Vector"):
            tn, cn = self.array_ref(c)
            kids = p.get(0, [])
            i = self.value(kids[0], depth + 1, seen) if kids else ""
            return f"{tn}.{cn}[{i}]"

        if t == "ValueOperator":
            args = [self.value(a, depth + 1, seen) for a in p.get(0, [])]
            return f"{c.name}({', '.join(args)})"

        if t in ("Value Vector", "Set Vector"):
            return self.label(c)

        args = [self.value(a, depth + 1, seen) for a in p.get(0, [])]
        return f"{self.label(c)}({', '.join(args)})" if args else self.label(c)

    # ------------------------------------------------------------ actions
    def action(self, idx, depth=0, seen=None, out=None, indent=0):
        out = [] if out is None else out
        seen = seen or set()
        c = self.g.get(idx)
        pad = "    " * indent
        if c is None:
            out.append(f"{pad}<#{idx}>")
            return out
        e = self.ext(c)
        if e:
            out.append(f"{pad}call {e}{self.bound_args(c, depth, seen)}")
            return out
        if idx in seen:
            out.append(f"{pad}{self.label(c)}()   // (already shown)")
            return out
        if depth > self.max_depth:
            out.append(f"{pad}{self.label(c)}()   // ... depth cut")
            return out
        seen = seen | {idx}
        p = self.ports(c)
        t = c.type_name

        if t == "ChannelCaller":
            kids = p.get(0, [])
            named = c.name and c.name not in GENERIC
            if named:
                out.append(f"{pad}// {c.name}  (#{c.index})")
            for k in kids:
                self.action(k, depth + 1, seen, out, indent)
            if not kids:
                out.append(f"{pad}{self.label(c)}()")
            return out

        if t in ("Set Value", "Set Text", "Set Vector"):
            src = p.get(0, [])
            tgts = p.get(1, [])
            rhs = self.value(src[0], depth + 1, seen) if src else (
                f'"{c.text}"' if t == "Set Text" else fmt_num(f32(c.tag("FLVA"))))
            names = [self._lhs(x) for x in tgts]
            if len(names) == 1:
                out.append(f"{pad}{names[0]} := {rhs}")
            else:
                out.append(f"{pad}{', '.join(names)} := {rhs}")
            return out

        if t == "If":
            cond = p.get(0, [])
            cs = self.value(cond[0], depth + 1, seen) if cond else "?"
            out.append(f"{pad}if {cs} {{")
            for k in p.get(1, []):
                self.action(k, depth + 1, seen, out, indent + 1)
            out.append(f"{pad}}}")
            return out

        if t == "IfElse":
            cond = p.get(0, [])
            cs = self.value(cond[0], depth + 1, seen) if cond else "?"
            out.append(f"{pad}if {cs} {{")
            for k in p.get(1, []):
                self.action(k, depth + 1, seen, out, indent + 1)
            out.append(f"{pad}}} else {{")
            for k in p.get(2, []):
                self.action(k, depth + 1, seen, out, indent + 1)
            out.append(f"{pad}}}")
            return out

        if t == "ForLoop":
            cnt = p.get(0, [])
            iv = p.get(1, [])
            cs = self.value(cnt[0], depth + 1, seen) if cnt else "?"
            ivn = self._lhs(iv[0]) if iv else "i"
            out.append(f"{pad}for {ivn} in 0..{cs} {{")
            for k in p.get(2, []):
                self.action(k, depth + 1, seen, out, indent + 1)
            out.append(f"{pad}}}")
            return out

        if t in ("CallSelected", "ChannelSwitch"):
            sel = p.get(0, [])
            ss = self.value(sel[0], depth + 1, seen) if sel else "?"
            cases = p.get(2, []) if t == "CallSelected" else p.get(1, [])
            dflt = p.get(1, []) if t == "CallSelected" else []
            out.append(f"{pad}switch {ss} {{")
            for j, k in enumerate(cases):
                kc = self.g.get(k)
                out.append(f"{pad}    case {j}: {self.label(kc)}")
            for k in dflt:
                out.append(f"{pad}    default: {self.label(self.g.get(k))}")
            out.append(f"{pad}}}")
            return out

        if t == "Array Command":
            acc = c.tag("ACCS")
            code = struct.unpack("<i", acc)[0] if acc and len(acc) == 4 else -1
            tgt = ", ".join(self._lhs(x) for x in p.get(0, []))
            out.append(f"{pad}{c.name} [{code}] ({tgt})")
            return out

        if t in ("Value", "Expression Value", "Array Value"):
            out.append(f"{pad}eval {self.value(idx, depth, seen)}")
            return out

        kids = p.get(0, [])
        out.append(f"{pad}{self.label(c)}   // {t}")
        for k in kids:
            self.action(k, depth + 1, seen, out, indent + 1)
        return out

    def _lhs(self, idx):
        c = self.g.get(idx)
        if c is None:
            return f"<#{idx}>"
        e = self.ext(c)
        if e:
            return e
        if c.type_name in ("Array Value", "Array Vector"):
            tn, cn = self.array_ref(c)
            kids = self.ports(c).get(0, [])
            i = self.value(kids[0], 0, set()) if kids else ""
            return f"{tn}.{cn}[{i}]"
        return self.label(c)


def _sub_operand(expr, letter, repl):
    """Replace a standalone operand letter (A/B/C...) in a Quest3D formula."""
    out = []
    i = 0
    n = len(expr)
    while i < n:
        ch = expr[i]
        if ch.upper() == letter:
            prev = expr[i - 1] if i else ""
            nxt = expr[i + 1] if i + 1 < n else ""
            if not (prev.isalnum() or prev == "_") and not (nxt.isalnum() or nxt == "_"):
                out.append(repl)
                i += 1
                continue
        out.append(ch)
        i += 1
    return "".join(out)
