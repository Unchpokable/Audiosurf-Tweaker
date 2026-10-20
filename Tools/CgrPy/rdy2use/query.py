r"""Ask the channel graph the questions every investigation starts with. Needs the cache (`cgrpy build_proj`).

    cgrpy query find     <regex> [--group G] [--type T] [--stubs]   channels whose name matches, in every group
    cgrpy query dec      <group> <channel> [--depth N]              a handler as pseudocode
    cgrpy query value    <group> <channel>                          what a channel evaluates to, as an expression
    cgrpy query writers  <group> <channel>                          every Set Value / Set Text that writes it,
                                                                     in its own group and through imports elsewhere
    cgrpy query switch   <group> <channel>                          a ChannelSwitch: its selector and each branch
    cgrpy query parents  <group> <channel>                          what a channel is wired into
    cgrpy query users    <name>                                     every group that imports a channel by that name

A group is a path, a file name or a short name (`SpecialPurpose`, `StartGroup`). A channel is an index
(`798` or `#798`) or a name; a name that more than one channel carries is refused with their indices,
because guessing which one you meant is how a wrong answer looks right.

**What the output means.** `dec` prints the graph as code: `X := expr` is a Set Value, `call G::Do_Y(...)`
a call into another group with its parameters bound, `switch S { case N: ... }` a ChannelSwitch or
CallSelected, `for i in 0..N` a ForLoop. `... depth cut` marks where the walk stopped: raise `--depth`, or
`dec` that index on its own. `A`, `B`, `C` inside an expression are its operands that were not expanded.

Where it was used: every number in Docs/internals/playing-audiosurf.md that was checked against the
graph - Pointman's stack depth (`switch SpecialPurpose 1525`, then 1052), the colour values
(`switch StatCollector 128`: 1 2 3 5 8), Eraser's Undo (`dec EraserEffect Do_FireUndo`), and who sets
`ChosenDifficulty` (`writers StartGroup ChosenDifficulty` - the menu, plus a slider no player has seen).
https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/internals/playing-audiosurf.md

Names lie. Half of the game's channels are called after something they no longer do, or after nothing:
a name is where to start looking, and `writers`, `parents` and `dec` are how you find out what it is.
"""

import re
import sys

from cgr.cache import load_project
from cgr.decomp import Decompiler
from cgr.project import ext_target

_P = None


def P():
    global _P
    if _P is None:
        _P = load_project()
    return _P


def channel(g, spec):
    """A channel by index or by unambiguous name."""
    text = str(spec).lstrip("#")
    if text.isdigit():
        c = g.get(int(text))
        if c is None:
            raise KeyError(f"no channel #{text} in {g.rel}")
        return c
    hits = g.by_name.get(spec, [])
    if not hits:
        near = sorted({c.name for c in g.channels.values() if c.name and spec.lower() in c.name.lower()})[:8]
        hint = f" Did you mean: {', '.join(near)}?" if near else ""
        raise KeyError(f"no channel called {spec!r} in {g.rel}.{hint}")
    if len(hits) > 1:
        listed = ", ".join(f"#{c.index} [{c.type_name}]" for c in hits)
        raise KeyError(f"{len(hits)} channels in {g.rel} are called {spec!r}: {listed} - give the index")
    return hits[0]


def _label(c):
    value = f" = {c.value:g}" if c.type_name == "Value" and c.value is not None else ""
    text = f" text={c.text!r}" if c.text else ""
    return f"#{c.index} [{c.type_name}] {c.name!r}{value}{text}"


def find(pattern, *options):
    groups = types = None
    stubs = False
    rest = list(options)
    while rest:
        flag = rest.pop(0)
        if flag == "--group":
            groups = rest.pop(0).lower()
        elif flag == "--type":
            types = rest.pop(0)
        elif flag == "--stubs":
            stubs = True
        else:
            raise KeyError(f"unknown option {flag!r}")
    rx = re.compile(pattern, re.I)
    count = 0
    for rel, g in sorted(P().groups.items()):
        if groups and groups not in rel.lower():
            continue
        for c in sorted(g.channels.values(), key=lambda c: c.index):
            if not c.name or not rx.search(c.name):
                continue
            if types and c.type_name != types:
                continue
            target = ext_target(c)
            if target and not stubs:
                continue
            via = f"  (import of {target[1]}::{target[0]})" if target else ""
            print(f"  {rel}  {_label(c)}{via}")
            count += 1
    print(f"{count} match(es)" + ("" if stubs else "; imports hidden - add --stubs to see them"))


def dec(group, spec, *options):
    depth = int(options[1]) if len(options) >= 2 and options[0] == "--depth" else 6
    g = P().find_group(group)
    c = channel(g, spec)
    out = []
    Decompiler(P(), g, max_depth=depth).action(c.index, out=out)
    print(f"--- {g.rel} {_label(c)}")
    for line in out:
        print("   ", line)


def value(group, spec):
    g = P().find_group(group)
    c = channel(g, spec)
    print(f"{g.rel} {_label(c)}\n  = {Decompiler(P(), g, max_depth=6).value(c.index)}")


def writers(group, spec):
    g0 = P().find_group(group)
    target = channel(g0, spec)
    count = 0
    # Names an import may use for g0: its file, its stem, and "StartGroup" for the root group.
    aliases = {g0.file, P().path_stem(g0.file)} | ({"StartGroup"} if g0.rel == "XX_StartHere.cgr" else set())
    for rel, g in sorted(P().groups.items()):
        # Imports of the channel - in other groups, and in its own: XX_StartHere writes
        # StartGroup::GoldRequirement through a stub that points back at itself.
        wanted = {c.index for c in g.channels.values()
                  if (t := ext_target(c)) and t[0] == target.name and P().path_stem(t[1]) in aliases}
        if g is g0:
            wanted.add(target.index)
        if not wanted:
            continue
        D = Decompiler(P(), g, max_depth=3)
        for c in sorted(g.channels.values(), key=lambda c: c.index):
            if c.type_name not in ("Set Value", "Set Text"):
                continue
            ports = g.ports(c)
            if wanted & set(ports.get(1, [])):
                source = ports.get(0, [])
                rhs = D.value(source[0]) if source else f"{c.value:g}" if c.value is not None else "?"
                print(f"  {rel} #{c.index}: {target.name} := {rhs}")
                count += 1
    print(f"{count} writer(s) of {g0.rel} {_label(target)}")


def switch(group, spec):
    g = P().find_group(group)
    c = channel(g, spec)
    D = Decompiler(P(), g, max_depth=3)
    print(f"{g.rel} {_label(c)}")
    ports = g.ports(c)
    for i in ports.get(0, []):
        print(f"  selector: #{i} = {D.value(i)}")
    for n, i in enumerate(ports.get(1, [])):
        print(f"  [{n}] #{i} {g.get(i).name!r} = {D.value(i)}")
    for p, kids in ports.items():
        if p > 1:
            print(f"  port {p}: " + ", ".join(f"#{k}" for k in kids))


def parents(group, spec):
    g = P().find_group(group)
    c = channel(g, spec)
    found = 0
    for p in sorted(g.channels.values(), key=lambda p: p.index):
        for link in p.links:
            if link.child == c.index:
                print(f"  port {link.ul} of {_label(p)}")
                found += 1
    if not found:
        print("  nothing in its own group - try `cgrpy query users` with its name")


def users(name):
    count = 0
    for rel, g in sorted(P().groups.items()):
        for c in sorted(g.channels.values(), key=lambda c: c.index):
            t = ext_target(c)
            if t and t[0] == name:
                print(f"  {rel} #{c.index} imports {t[1]}::{t[0]}")
                count += 1
    print(f"{count} import(s) of {name!r}")


COMMANDS = dict(find=find, dec=dec, value=value, writers=writers, switch=switch, parents=parents, users=users)

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    if len(sys.argv) < 3 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        raise SystemExit(2)
    try:
        COMMANDS[sys.argv[1]](*sys.argv[2:])
    except (KeyError, IndexError, ValueError, re.error) as error:
        raise SystemExit(f"query: {error.args[0] if error.args else error}")
