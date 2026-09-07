"""L0/L1 container: unwrap a .cgr to its L2 chunk stream and rewrap it losslessly.

Three shipped layouts (census over engine/, 161 files):

    zlib+xor    109   ACTF/ZIOS/ZINS/ZICB  ->  ACTF/NECL/NECT/NEOS/NECB  ->  QVRS...
    zlib only    42   ACTF/ZIOS/ZINS/ZICB  ->  QVRS...
    plain        10   QVRS...

  ACTF = GUID of the FileConvert channel that encoded the layer
         7DFC389A-... ZipCompression, FBB1D22B-... NoEditorLoad
  ZIOS = inflated size      ZINS = deflated size (== len(ZICB))
  NECL = len(NECT)          NECT = b"Protected files"
  NEOS = len(NECB)          NECB = payload XOR 0x04

`rewrap(unwrap(b)) ` reproduces the inner layer byte-for-byte on all 109
xor files; the outer zlib stream differs only by deflate level, which the
loader does not care about.
"""

import struct
import zlib

from .core import iter_chunks


def _chunk(tag, payload):
    return tag.encode("ascii") + struct.pack("<I", len(payload)) + payload


class Envelope:
    """Remembers exactly how a file was wrapped so it can be rebuilt."""

    def __init__(self, kind, outer_guid=b"", inner_guid=b"", nect=b"Protected files"):
        self.kind = kind  # "plain" | "zlib" | "zlib+xor"
        self.outer_guid = outer_guid
        self.inner_guid = inner_guid
        self.nect = nect

    def __repr__(self):
        return f"<Envelope {self.kind}>"


def unwrap_file(path):
    """-> (core_L2_bytes, Envelope)"""
    from pathlib import Path
    b = Path(path).read_bytes()
    if b[:4] == b"QVRS":
        return b, Envelope("plain")
    ch = {c.tag: c for c in iter_chunks(b)}
    og = b[ch["ACTF"].off:ch["ACTF"].off + 16]
    raw = zlib.decompress(b[ch["ZICB"].off:ch["ZICB"].off + ch["ZICB"].length])
    ich = {c.tag: c for c in iter_chunks(raw)}
    if "NECB" not in ich:
        return raw, Envelope("zlib", outer_guid=og)
    core = bytes(x ^ 4 for x in raw[ich["NECB"].off:ich["NECB"].off + ich["NECB"].length])
    return core, Envelope(
        "zlib+xor",
        outer_guid=og,
        inner_guid=raw[ich["ACTF"].off:ich["ACTF"].off + 16],
        nect=raw[ich["NECT"].off:ich["NECT"].off + ich["NECT"].length],
    )


def rewrap(core, env, level=6):
    """Inverse of unwrap_file. Returns the full .cgr file bytes."""
    if env.kind == "plain":
        return core
    if env.kind == "zlib+xor":
        inner = _chunk("ACTF", env.inner_guid)
        inner += _chunk("NECL", struct.pack("<I", len(env.nect)))
        inner += _chunk("NECT", env.nect)
        inner += _chunk("NEOS", struct.pack("<I", len(core)))
        inner += _chunk("NECB", bytes(x ^ 4 for x in core))
    else:
        inner = core
    comp = zlib.compress(inner, level)
    return (_chunk("ACTF", env.outer_guid)
            + _chunk("ZIOS", struct.pack("<I", len(inner)))
            + _chunk("ZINS", struct.pack("<I", len(comp)))
            + _chunk("ZICB", comp))
