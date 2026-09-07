"""Find vtables a module does NOT export, by their shape.

    uvrun findvt <module>                       # runs of >= 8 consecutive code pointers
    uvrun findvt <module> --min 23              # at least this many slots
    uvrun findvt <module> --ret 20:4 --ret 22:0xc  # ...and these slots end in `ret <imm>`

Not every class publishes `??_7Name@@6B@`. An internal one has no symbol at all, so the only handle
on it is what its table looks like: a run of pointers into `.text`, at a `.rdata` address nothing
names.

**The `--ret` filter is what makes this usable rather than a list of candidates.** On x86 a
`__thiscall` method cleans its own arguments, so the immediate on its `ret` counts them: `ret 4` is
one dword, `ret 0xc` is three. Knowing the shape of two slots from the call site is normally enough
to identify a class uniquely among everything in a module.

**The slot count is a run length, not a table length.** MSVC lays vtables back to back in `.rdata`
with nothing between them, so a run of code pointers can span several classes and the walk cannot see
the seam. Read the reported address with `vtdump` and find the real end by the names: the class
prefix changes (`?...@ArrayConnectItem@@` giving way to `?...@Aco_Array_Unique@@` is where
ArrayConnectItem's 24 slots stop), or the type's own name appears as a string. Taking the run length
as the table length is how a class acquires methods that belong to its neighbour.

Where it was used: `ArrayConnectItem`, the object an Array Value/Vector channel keeps its table
through. It is implemented in the `Array Unique` DLL and exported by nothing. The call sites showed
slot 20 taking one argument and slot 22 taking three, so:

    uvrun findvt --type "Array Unique" --min 23 --ret 20:4 --ret 22:0xc

returned exactly one candidate, whose slot 0 turned out to be an exported
`?GetIfValid@ArrayConnectItem@@...` - which named the class and gave the whole 24-slot table. That
table is where `GetRow` and `GetRowOrCreate` live, one slot apart, and telling them apart is the
difference between a bounds check and silently growing the game's tables.
"""

import sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs

from cgr import pe
from cgr.paths import channel_type_dll

_md = Cs(CS_ARCH_X86, CS_MODE_32)


def ret_immediate(image, rva: int, limit: int = 200):
    """The immediate on the function's first `ret`, or None if it tail-jumps or runs long."""
    offset = image.rva_to_offset(rva)
    if offset is None:
        return None

    for insn in _md.disasm(image.data[offset:offset + limit * 8], image.image_base + rva):
        if insn.mnemonic == "ret":
            return int(insn.op_str, 0) if insn.op_str else 0
        if insn.mnemonic == "jmp":
            return None

    return None


def parse_ret_spec(spec: str) -> dict:
    """`20:4` or `20=4`, optionally several separated by commas.

    Both separators are accepted because **cmd treats `=` and `,` as argument delimiters**, so
    `--ret 20=4,22=0xc` arrives shredded into pieces unless it is quoted. The colon form with one
    flag per slot needs no quoting and is the one to reach for on Windows:

        --ret 20:4 --ret 22:0xc
    """
    out = {}
    for part in spec.replace(";", ",").split(","):
        part = part.strip()
        if not part:
            continue

        slot, sep, value = part.partition(":")
        if not sep:
            slot, sep, value = part.partition("=")
        if not sep or not value.strip():
            raise SystemExit(f"--ret wants <slot>:<ret imm>, got {part!r}. On Windows prefer 20:4 over 20=4 (cmd eats '=').")

        out[int(slot.strip())] = int(value.strip(), 0)

    return out


def main(*args):
    args = list(args)

    if args[0] == "--type":
        path = channel_type_dll(args[1])
        del args[:2]
    else:
        path = args.pop(0)

    minimum = 8
    wanted = {}
    while args:
        flag = args.pop(0)
        if flag == "--min":
            minimum = int(args.pop(0))
        elif flag == "--ret":
            wanted.update(parse_ret_spec(args.pop(0)))
        else:
            raise SystemExit(f"unknown option {flag!r}")

    if wanted:
        minimum = max(minimum, max(wanted) + 1)

    image = pe.load(path)
    print(f"=== {image.path.name}  image base 0x{image.image_base:08x}")

    found = 0
    for section in image.sections:
        if not (section.name.startswith(".rdata") or section.name.startswith(".data")):
            continue

        slot_count = section.size // 4
        index = 0
        while index < slot_count:
            run = image.vtable(section.rva + index * 4, limit=slot_count - index)
            if len(run) >= minimum and _matches(image, run, wanted):
                rva = section.rva + index * 4
                first = image.va_to_rva(run[0])
                names = ", ".join(image.exports.get(first, []))
                print(f"  {section.name}: rva 0x{rva:05x}  {len(run)} slots  slot0=0x{first:05x} {names}")
                found += 1

            index += max(len(run), 1)

    if found == 0:
        print("  nothing matched - try lowering --min, or check the ret shape at the call site")


def _matches(image, run, wanted) -> bool:
    for slot, expected in wanted.items():
        if slot >= len(run):
            return False
        if ret_immediate(image, image.va_to_rva(run[slot])) != expected:
            return False
    return True


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        raise SystemExit(2)
    main(*sys.argv[1:])
