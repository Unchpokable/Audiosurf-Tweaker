"""List what a module exports and imports.

    uvrun symbols <module>                 # exports
    uvrun symbols <module> vector          # exports whose name contains "vector"
    uvrun symbols <module> --imports       # imports, grouped by DLL
    uvrun symbols --type "Array Vector" --imports

**Imports are the cheap way to find a C++ type's parent.** A channel-type DLL imports its base
class's constructor and destructor from the base class's own DLL, so the import list answers "what
does this derive from" without decompiling anything. `Aco_Array_Vector` importing
`??0Aco_VectorChannel@@QAE@XZ` from `9D045960-....dll` is how its family was confirmed - and with it,
which vtable slots it inherits and which are its own.

Exports matter for the opposite reason: they are how a slot address in a vtable dump turns into a
name. `vtdump` does that lookup for you; this is for when you want to search the other way, "does
this module export anything called Table".
"""

import sys

from cgr import pe
from cgr.paths import channel_type_dll

from vtdump import find_module


def main(*args):
    args = list(args)

    if args[0] == "--type":
        path = channel_type_dll(args[1])
        del args[:2]
    else:
        path = find_module(args.pop(0))

    show_imports = "--imports" in args
    args = [a for a in args if a != "--imports"]
    needle = args[0].lower() if args else ""

    image = pe.load(path)
    print(f"=== {image.path.name}  image base 0x{image.image_base:08x}\n")

    if show_imports:
        for dll, symbols in sorted(image.imports().items()):
            shown = [s for s in symbols if needle in s.lower()]
            if not shown:
                continue
            print(f"{dll}:")
            for symbol in sorted(shown):
                print(f"    {symbol}")
            print()
        return

    rows = [(rva, name) for name, rva in image.exports_by_name.items() if needle in name.lower()]
    for rva, name in sorted(rows):
        print(f"0x{rva:05x}  {name}")

    print(f"\n{len(rows)} of {len(image.exports_by_name)} exports", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        raise SystemExit(2)
    main(*sys.argv[1:])
