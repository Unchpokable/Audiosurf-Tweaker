"""Dump a C++ vtable out of a module, with slot names resolved from the export table.

    uvrun vtdump <module>                    # every exported ??_7<Class>@@6B@ table
    uvrun vtdump <module> 0x3130             # the table at that rva
    uvrun vtdump --type "Array Vector"       # look the module up by channel type name

`<module>` is a path, or a bare file name that will be looked for in `engine/` and
`engine/channels/`.

Reading the output: an unnamed slot is either inherited (a 6-byte `jmp [iat]` thunk - disassemble it
to see whose) or simply not exported. The line after the last slot prints the bytes that stopped the
walk; for an MSVC vtable those are usually the type's own name, which confirms you read the right
table and reached its real end.

**The slot count is a run length, not a table length.** MSVC lays vtables back to back in `.rdata`
with nothing between them and the walk cannot see the seam, so a dump can run past the end of the
class you asked for. Find the real end by the names: the class prefix changes (`?...@ArrayConnectItem@@` giving way to `?...@Aco_Array_Unique@@` is where
ArrayConnectItem's 24 slots stop), or the type's own name appears as a string. Taking the run length
as the table length is how a class acquires methods that belong to its neighbour.

Where it was used: `Aco_Array_Value` and `Aco_Array_Vector` (engine journal §2.2.3). The question was
whether either overrides the setter its family already had, because if they inherited it a table
write would land in the channel's own scalar and the table would never move. Both override. That one
dump is what unblocked writing to tables.
"""

import sys

from cgr import pe
from cgr.paths import channel_type_dll, channels_dir, engine_dir


def find_module(name: str):
    from pathlib import Path

    direct = Path(name)
    if direct.is_file():
        return direct

    for folder in (engine_dir(), channels_dir()):
        candidate = folder / name
        if candidate.is_file():
            return candidate

    raise FileNotFoundError(f"no module called {name!r} in engine/ or engine/channels/")


def dump_one(image, name: str, rva: int, limit: int):
    print(f"-- {name} @ rva 0x{rva:x}")

    slots = image.vtable(rva, limit)
    for i, va in enumerate(slots):
        slot_rva = image.va_to_rva(va)
        names = ", ".join(image.exports.get(slot_rva, []))
        print(f"   [{i:2}] +0x{i * 4:02x} rva=0x{slot_rva:05x}  {names}")

    offset = image.rva_to_offset(rva) + len(slots) * 4
    tail = image.data[offset:offset + 24]
    print(f"   [{len(slots):2}] end of table; next bytes: {tail!r}\n")


def main(*args):
    limit = 64
    if args and args[0] == "--type":
        path = channel_type_dll(args[1])
        rest = args[2:]
    else:
        path = find_module(args[0])
        rest = args[1:]

    image = pe.load(path)
    print(f"=== {path.name}  image base 0x{image.image_base:08x}\n")

    if rest:
        dump_one(image, "requested", int(rest[0], 0), limit)
        return

    exported = image.vtable_exports()
    if not exported:
        print("no exported vtable symbols; use findvt to search by shape")
        return

    for symbol in exported:
        dump_one(image, symbol, image.exports_by_name[symbol], limit)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        raise SystemExit(2)
    main(*sys.argv[1:])
