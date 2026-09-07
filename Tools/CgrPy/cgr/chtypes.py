"""`engine/channels.lst` - the map from a channel type's display name to the DLL implementing it.

The file is a flat sequence of records. Each carries a `CHTY` tag followed by the type name, and a
`C1FN` tag followed by the DLL file name, both in fixed-width padded fields. There are 226 of them,
which matches the type count the engine journal arrived at independently.

This is the first step of almost any binary question about a channel: "Array Vector" means nothing to
a disassembler, `DD626E09-....dll` does.
"""

import re

from cgr.paths import engine_dir

_RECORD = re.compile(rb"CHTY")
_FIELD_MAX = 80


def entries() -> list:
    """`[(type name, dll file name), ...]` in file order."""
    data = (engine_dir() / "channels.lst").read_bytes()

    out = []
    for match in _RECORD.finditer(data):
        # 4 bytes of length follow the tag; the name is a padded field after it.
        start = match.end() + 4
        name = data[start:start + _FIELD_MAX].split(b"\0")[0].decode("latin1").strip()

        file_tag = data.find(b"C1FN", start)
        if file_tag < 0:
            continue
        dll = data[file_tag + 8:file_tag + 8 + _FIELD_MAX].split(b"\0")[0].decode("latin1").strip()

        out.append((name, dll))

    return out


def type_to_dll() -> dict:
    return dict(entries())
