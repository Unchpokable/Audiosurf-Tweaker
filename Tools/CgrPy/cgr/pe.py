"""Minimal PE reader, enough to answer questions about the game's DLLs.

Not a general PE library. It does the four things the reverse-engineering notes actually needed:
map an RVA to a file offset, list exports, list imports, and read a vtable out of `.rdata`.

**Why hand-rolled rather than pefile.** These files are read alongside a disassembler and a set of
notes that quote raw RVAs; every layer that renames or normalises something is a layer where an
address in the journal stops matching an address on screen. Sixty lines that speak the same
coordinates as the journal are worth more here than a dependency that speaks its own.

Everything is offline and read-only. Nothing here loads or executes a module.

Vocabulary, because the two are constantly confused:

* **RVA** - offset from the module's image base, what a disassembler shows and what the journals
  quote.
* **file offset** - where that byte actually sits in the file on disk.

`Image.rva_to_offset` is the only conversion, and it returns `None` for an RVA in no section rather
than guessing - which is how the end of a vtable is detected.
"""

import struct
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Section:
    name: str
    rva: int
    virtual_size: int
    file_offset: int
    raw_size: int

    @property
    def size(self) -> int:
        # Sections are padded on disk and can also be larger in memory than on disk. Taking the max
        # keeps a lookup inside a section that is bigger either way.
        return max(self.virtual_size, self.raw_size)


@dataclass
class Image:
    path: Path
    data: bytes
    image_base: int
    sections: list
    _data_dir: int
    _exports_by_rva: dict = field(default_factory=dict, repr=False)
    _exports_by_name: dict = field(default_factory=dict, repr=False)
    _iat: dict = field(default_factory=dict, repr=False)
    _loaded_symbols: bool = field(default=False, repr=False)

    # --- addressing ---------------------------------------------------------------------------

    def rva_to_offset(self, rva: int):
        for section in self.sections:
            if section.rva <= rva < section.rva + section.size:
                return section.file_offset + (rva - section.rva)
        return None

    def va_to_rva(self, va: int) -> int:
        return va - self.image_base

    def is_code(self, va: int) -> bool:
        """Whether a virtual address points into the executable section."""
        text = next((s for s in self.sections if s.name.startswith(".text")), None)
        if text is None:
            return False
        rva = self.va_to_rva(va)
        return text.rva <= rva < text.rva + text.size

    def u32(self, rva: int) -> int:
        offset = self.rva_to_offset(rva)
        if offset is None:
            raise ValueError(f"rva 0x{rva:x} is not inside any section of {self.path.name}")
        return struct.unpack_from("<I", self.data, offset)[0]

    def cstring(self, offset: int) -> str:
        end = self.data.index(b"\0", offset)
        return self.data[offset:end].decode("latin1")

    # --- symbols ------------------------------------------------------------------------------

    def _ensure_symbols(self):
        if not self._loaded_symbols:
            self._read_exports()
            self._read_imports()
            self._loaded_symbols = True

    @property
    def exports(self) -> dict:
        """`{rva: [name, ...]}`. A list because MSVC folds identical functions together, and seeing
        that two names share an address is itself informative - it is how `Release` was found to be
        the same code as `CallChannel`."""
        self._ensure_symbols()
        return self._exports_by_rva

    @property
    def exports_by_name(self) -> dict:
        self._ensure_symbols()
        return self._exports_by_name

    @property
    def iat(self) -> dict:
        """`{virtual address of the IAT slot: "dll!symbol"}`, for naming `call [0x...]`."""
        self._ensure_symbols()
        return self._iat

    def _read_exports(self):
        table_rva, _ = struct.unpack_from("<II", self.data, self._data_dir)
        if table_rva == 0:
            return

        base = self.rva_to_offset(table_rva)
        _func_count, name_count = struct.unpack_from("<II", self.data, base + 20)
        func_rva, name_rva, ord_rva = struct.unpack_from("<III", self.data, base + 28)

        func_off = self.rva_to_offset(func_rva)
        name_off = self.rva_to_offset(name_rva)
        ord_off = self.rva_to_offset(ord_rva)

        for i in range(name_count):
            entry_name_rva = struct.unpack_from("<I", self.data, name_off + i * 4)[0]
            ordinal = struct.unpack_from("<H", self.data, ord_off + i * 2)[0]
            target = struct.unpack_from("<I", self.data, func_off + ordinal * 4)[0]
            name = self.cstring(self.rva_to_offset(entry_name_rva))
            self._exports_by_rva.setdefault(target, []).append(name)
            self._exports_by_name[name] = target

    def _read_imports(self):
        table_rva, _ = struct.unpack_from("<II", self.data, self._data_dir + 8)
        if table_rva == 0:
            return

        cursor = self.rva_to_offset(table_rva)
        while True:
            lookup, _stamp, _fwd, name_rva, address_rva = struct.unpack_from("<IIIII", self.data, cursor)
            if name_rva == 0:
                break

            dll = self.cstring(self.rva_to_offset(name_rva))
            entry = self.rva_to_offset(lookup or address_rva)
            slot = address_rva
            while True:
                value = struct.unpack_from("<I", self.data, entry)[0]
                if value == 0:
                    break
                if value & 0x80000000:
                    symbol = f"{dll}!#{value & 0xFFFF}"
                else:
                    # An import-by-name entry is a 2-byte hint followed by the name.
                    symbol = f"{dll}!{self.cstring(self.rva_to_offset(value) + 2)}"
                self._iat[slot + self.image_base] = symbol
                entry += 4
                slot += 4

            cursor += 20

    def imports(self) -> dict:
        """`{dll: [symbol, ...]}` - the shape that answers "what does this type derive from",
        because a channel DLL imports its parent's constructor from the parent's DLL."""
        by_dll = {}
        for symbol in self.iat.values():
            dll, _, name = symbol.partition("!")
            by_dll.setdefault(dll, []).append(name)
        return by_dll

    # --- vtables ------------------------------------------------------------------------------

    def vtable(self, rva: int, limit: int = 64) -> list:
        """Slot virtual addresses starting at `rva`, stopping at the first entry that is not code.

        That stop condition is the real end-of-table detector: MSVC lays vtables out back to back in
        `.rdata` followed by the type's name as a plain string, so the first non-pointer is reliably
        the end. Compare `vtdump.py`, which prints the bytes it stopped on - they are usually the
        type name and confirm you read the right table.
        """
        offset = self.rva_to_offset(rva)
        if offset is None:
            raise ValueError(f"rva 0x{rva:x} is not inside any section of {self.path.name}")

        slots = []
        for i in range(limit):
            va = struct.unpack_from("<I", self.data, offset + i * 4)[0]
            if not self.is_code(va) or self.rva_to_offset(self.va_to_rva(va)) is None:
                break
            slots.append(va)

        return slots

    def vtable_exports(self) -> list:
        """Every exported `??_7<Class>@@6B@` symbol - the vtables the module publishes."""
        return sorted(name for name in self.exports_by_name if name.startswith("??_7"))


def load(path) -> Image:
    path = Path(path)
    data = path.read_bytes()

    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError(f"{path} is not a PE file")

    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    data_dir = opt + (96 if magic == 0x10B else 112)

    sections = []
    table = opt + opt_size
    for i in range(section_count):
        entry = table + i * 40
        name = data[entry:entry + 8].rstrip(b"\0").decode("latin1")
        virtual_size, rva, raw_size, file_offset = struct.unpack_from("<IIII", data, entry + 8)
        sections.append(Section(name, rva, virtual_size, file_offset, raw_size))

    return Image(path=path, data=data, image_base=image_base, sections=sections, _data_dir=data_dir)
