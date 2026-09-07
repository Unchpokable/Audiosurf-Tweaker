"""Disassemble a function, with calls resolved to names.

    uvrun disasm <module> 0x1240              # stops at the first `ret`
    uvrun disasm <module> 0x1240 --count 80
    uvrun disasm --type "Array Vector" 0x1240

Plain capstone output for x86, plus the one thing that makes it readable here: `call [0x...]` through
the import table is annotated with the imported symbol, and a direct `call` to an exported address is
annotated with its name. Without that, a Quest3D channel method is a wall of indirect calls through
offsets, and the interesting question is almost always *which* method is being called.

Where it was used: reading `Aco_Array_Vector::SetVector` to answer two questions at once - where the
row index comes from (`GetChild(0)` then the child's slot 17, truncated to int) and whether the
argument is a `D3DXVECTOR3` by value (three consecutive dwords from `[esp+4]`, so the plugin's
`(self, edx, float, float, float)` thunk is byte-identical). The second question is the kind that
cannot be settled by reasoning about the ABI: guess wrong and it is a stack imbalance, not a wrong
number. Engine journal §2.2.2 and §2.2.3.
"""

import sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs

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

    rva = int(args.pop(0), 0)

    count = 64
    stop_at_ret = True
    while args:
        flag = args.pop(0)
        if flag == "--count":
            count = int(args.pop(0))
        elif flag == "--all":
            stop_at_ret = False
        else:
            raise SystemExit(f"unknown option {flag!r}")

    image = pe.load(path)
    offset = image.rva_to_offset(rva)
    if offset is None:
        raise SystemExit(f"rva 0x{rva:x} is not inside any section of {image.path.name}")

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for insn in md.disasm(image.data[offset:offset + count * 8], image.image_base + rva):
        line = f"  {insn.address:08x}  {insn.mnemonic:<8} {insn.op_str}"
        line += _annotate(image, insn)
        print(line)

        if stop_at_ret and insn.mnemonic == "ret":
            break


def _annotate(image, insn) -> str:
    # Direct call/jmp to something the module exports.
    if insn.mnemonic in ("call", "jmp") and insn.op_str.startswith("0x"):
        target = image.va_to_rva(int(insn.op_str, 16))
        names = image.exports.get(target)
        if names:
            return f"   ; {names[0]}"

    # Indirect through the import address table.
    if "dword ptr [0x" in insn.op_str:
        try:
            address = int(insn.op_str.split("[0x")[1].split("]")[0], 16)
        except (IndexError, ValueError):
            return ""
        symbol = image.iat.get(address)
        if symbol:
            return f"   ; {symbol}"

    return ""


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(2)
    main(*sys.argv[1:])
