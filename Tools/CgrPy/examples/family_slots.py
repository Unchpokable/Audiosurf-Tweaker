r"""Which channel types override the slots a family accessor calls - and whether a type's family by guid matches
its family by inheritance.

    cgrpy examples\family_slots.py            # every type that is not a plain inheritor
    cgrpy examples\family_slots.py --all      # every type

TweakerPlugin decides a channel's family from its type record (base guid at +0x60, own guid at +0x50 -
`engine/channel_kind.cxx`) and then calls fixed vtable slots on it: number 17 GetFloat / 19 SetFloat, vector
17 GetVector / 18 SetVector, text 17 GetString / 23 GetIfUseWChar / 24 GetWString, matrix 17 / 19. Two things
can make that wrong for one particular type, and neither is visible from the family alone:

  - the type overrides the slot, so "set" is the type's own action (Array Value's SetFloat writes a table row
    and creates it when missing);
  - the type claims a family by guid without deriving from that family's class, so the slot is a different
    method altogether.

`channels.lst` carries the same type record the engine hands out at runtime (name, then the type guid at +80
and the base guid at +96), so the plugin's view of every type can be had without running the game. The
inheritance side comes from the DLL: each slot of the exported `??_7Aco_...@@6B@` table is either the
module's own code (exported name or not) or a `jmp [iat]` thunk into the parent's DLL, which names the parent.

Where it was used: the scripting-layer audit (`lua-audit-journal.md`, A4 and A6), to list the types on which
`float_ch:set` / `vector_ch:set` / `:get` is an action rather than a store or a read.
https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/lua-audit-journal.md
"""
import re
import struct
import sys
import uuid

from cgr import pe
from cgr.paths import channels_dir, engine_dir

FAMILIES = {
    "BE69CCC4-CFC1-4362-AC81-767D199BBFC3": "number",
    "9D045960-EAC2-4C40-9BBF-10F32F7FA305": "vector",
}
# The plugin's other two families are found by the name of their base class below, not hardcoded here.
FAMILY_BASE = {
    "number": "Aco_FloatChannel",
    "vector": "Aco_VectorChannel",
    "text": "Aco_StringChannel",
    "matrix": "Aco_MatrixChannel",
}
SLOTS = {
    "number": {17: "GetFloat", 19: "SetFloat"},
    "vector": {17: "GetVector", 18: "SetVector"},
    "text": {17: "GetString", 23: "GetIfUseWChar", 24: "GetWString"},
    "matrix": {17: "GetMatrix", 19: "SetMatrix"},
}


def channel_tables(image):
    """`[(label, rva)]` of the channel class vtables in a module.

    Exported `??_7Aco_...` tables when there are any. Audiosurf's own channel DLLs (Texter_Unicode, XML_*,
    Unicode_*) export none, so for them every run of >= 18 code pointers in `.rdata` that thunks into
    A3d_Channel somewhere is taken - those are channel classes and nothing else in such a module is.
    """
    exported = [s for s in image.vtable_exports() if s.startswith("??_7Aco_")]
    if exported:
        return [(s[4:].split("@@")[0], image.exports_by_name[s]) for s in exported]

    out = []
    for section in image.sections:
        if not section.name.startswith(".rdata"):
            continue
        i = 0
        while i < section.size - 4:
            rva = section.rva + i
            run = image.vtable(rva, 64) if image.rva_to_offset(rva) is not None else []
            if len(run) >= 18 and any(owner_of(image, va)[0] == "A3d_Channel" for va in run[:17]):
                out.append((f"(unexported @0x{rva:x})", rva))
                i += len(run) * 4
            else:
                i += 4
    return out


def records():
    data = (engine_dir() / "channels.lst").read_bytes()
    for m in re.finditer(rb"CHTY", data):
        start = m.end() + 4
        name = data[start:start + 80].split(b"\0")[0].decode("latin1").strip()
        own = str(uuid.UUID(bytes_le=data[start + 80:start + 96])).upper()
        base = str(uuid.UUID(bytes_le=data[start + 96:start + 112])).upper()
        tag = data.find(b"C1FN", start)
        dll = data[tag + 8:tag + 88].split(b"\0")[0].decode("latin1").strip()
        yield name, own, base, dll


def owner_of(image, va):
    """(class, method) of the code a vtable slot points at."""
    rva = image.va_to_rva(va)
    for name in image.exports.get(rva, []):
        m = re.match(r"\?(\w+)@(\w+)@@", name)
        if m:
            return m.group(2), m.group(1)
    off = image.rva_to_offset(rva)
    if off is not None and image.data[off:off + 2] == b"\xff\x25":
        slot = struct.unpack_from("<I", image.data, off + 2)[0]
        symbol = image.iat.get(slot, "")
        m = re.search(r"!\?(\w+)@(\w+)@@", symbol)
        if m:
            return m.group(2), m.group(1)
        return "?import", symbol
    return "(own)", f"rva 0x{rva:x}"


def main(*args):
    show_all = "--all" in args
    by_guid = {}  # base class name -> family guid, learnt from the family's own type
    rows = []
    for name, own, base, dll in records():
        path = channels_dir() / dll
        if not path.is_file():
            path = engine_dir() / dll
        if not path.is_file():
            continue
        image = pe.load(path)
        rows.append((name, own, base, dll, image))

    # Text and matrix: the family guid is whichever type owns the base class.
    for name, own, base, dll, image in rows:
        for symbol in image.vtable_exports():
            cls = symbol[4:].split("@@")[0]
            if cls == "Aco_StringChannel":
                FAMILIES.setdefault(own, "text")
            if cls == "Aco_MatrixChannel":
                FAMILIES.setdefault(own, "matrix")

    report = []
    for name, own, base, dll, image in rows:
        family = FAMILIES.get(base) or FAMILIES.get(own)
        if family is None or family not in SLOTS:
            continue
        tables = channel_tables(image)
        if not tables:
            report.append((family, name, "-", dll, ["no channel vtable found"]))
        for cls, rva in tables:
            slots = image.vtable(rva, 64)
            if len(slots) < 18:
                continue
            notes = []
            inherits = False
            for i, va in enumerate(slots[:40]):
                c, _ = owner_of(image, va)
                if c == FAMILY_BASE[family]:
                    inherits = True
            for slot, method in SLOTS[family].items():
                if slot >= len(slots):
                    notes.append(f"slot {slot} MISSING (table has {len(slots)})")
                    continue
                c, m = owner_of(image, slots[slot])
                if c != FAMILY_BASE[family]:
                    notes.append(f"{slot} {method} -> {c}::{m}")
            if cls == FAMILY_BASE[family]:
                inherits = True
            if not inherits:
                notes.insert(0, f"NOT derived from {FAMILY_BASE[family]} (by thunks)")
            if notes or show_all:
                report.append((family, name, cls, dll, notes))

    for family, name, cls, dll, notes in sorted(report):
        print(f"[{family}] {name}  ({cls}, {dll})")
        for n in notes:
            print(f"      {n}")
    print(f"\n{len(report)} type table(s) listed", file=sys.stderr)


if __name__ == "__main__":
    main(*sys.argv[1:])
