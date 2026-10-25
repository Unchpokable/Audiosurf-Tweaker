r"""Code that compares an object's vptr against a vtable address - every module of the game.

    cgrpy examples\vptr_compares.py

TweakerPlugin's channel_shim gives one channel object a private copy of its vtable (the vptr then points into
the plugin's heap, not into the type's `.rdata`). Everything that calls through the vptr is unaffected. What a
copy does break is identity by address: `if (*(void**)obj == &Aco_X::vftable)`. This looks for exactly that
shape in `.text` of every module under engine/ and engine/channels/:

  - `cmp <reg|[mem]>, imm32` where imm32 is a vtable of the same module (an exported `??_7...@@6B@`, or the
    start of a run of >= 8 code pointers in `.rdata`);
  - `cmp <reg>, dword ptr [iat]` where the import is a `??_7...` symbol of another module.

A hit is a place that would misidentify a hooked channel. Constructors and destructors *store* vtables with
`mov`, which is not reported. Where it was used: the scripting-layer audit (`lua-audit-journal.md`, open
questions). https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/lua-audit-journal.md
"""
import sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

from cgr import pe
from cgr.paths import channels_dir, engine_dir


def vtable_starts(image):
    starts = {image.image_base + rva for name, rva in image.exports_by_name.items() if name.startswith("??_7")}
    for section in image.sections:
        if not section.name.startswith(".rdata"):
            continue
        i = 0
        while i < section.size - 4:
            run = image.vtable(section.rva + i, 8) if image.rva_to_offset(section.rva + i) is not None else []
            if len(run) >= 8:
                starts.add(image.image_base + section.rva + i)
                i += 4 * len(image.vtable(section.rva + i, 256))
            else:
                i += 4
    return starts


def scan(path):
    image = pe.load(path)
    tables = vtable_starts(image)
    vt_imports = {va for va, sym in image.iat.items() if "!??_7" in sym}
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    md.skipdata = True
    hits = []
    for section in image.sections:
        if not section.name.startswith(".text"):
            continue
        off = image.rva_to_offset(section.rva)
        code = image.data[off:off + section.size]
        for insn in md.disasm(code, image.image_base + section.rva):
            if insn.mnemonic != "cmp" or len(insn.operands) != 2:
                continue
            a, b = insn.operands
            if b.type == X86_OP_IMM and (b.imm & 0xFFFFFFFF) in tables:
                hits.append((insn.address, f"{insn.mnemonic} {insn.op_str}"))
            elif b.type == X86_OP_MEM and b.mem.base == 0 and b.mem.index == 0 and (b.mem.disp & 0xFFFFFFFF) in vt_imports:
                hits.append((insn.address, f"{insn.mnemonic} {insn.op_str}  ; {image.iat[b.mem.disp & 0xFFFFFFFF]}"))
            elif a.type == X86_OP_MEM and a.mem.base == 0 and a.mem.index == 0 and (a.mem.disp & 0xFFFFFFFF) in vt_imports:
                hits.append((insn.address, f"{insn.mnemonic} {insn.op_str}  ; {image.iat[a.mem.disp & 0xFFFFFFFF]}"))
    return hits


def main():
    modules = sorted(channels_dir().glob("*.dll")) + sorted(engine_dir().glob("*.dll"))
    total = 0
    for path in modules:
        try:
            hits = scan(path)
        except Exception as error:  # a module the PE reader cannot walk is reported, not fatal
            print(f"!! {path.name}: {error}")
            continue
        for address, text in hits:
            print(f"{path.name}  0x{address:08x}  {text}")
        total += len(hits)
    print(f"\n{total} compare(s) in {len(modules)} module(s)", file=sys.stderr)


if __name__ == "__main__":
    main()
