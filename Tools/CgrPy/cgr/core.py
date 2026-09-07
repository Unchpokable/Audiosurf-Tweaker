"""Quest3D .cgr container + channel-graph reader for Audiosurf.

Layers (see Docs/Internal/reversing-journal-lua.md):
  L0: ACTF/ZIOS/ZINS/ZICB     ZICB payload = zlib stream
  L1: ACTF/NECL/NECT/NEOS/NECB  NECB payload = chunk stream XOR 0x04
  L2: chunk stream, TAG(4 ASCII) + u32 len + payload; DGCG/ESCH nest.
"""

import struct
import zlib
import uuid
from dataclasses import dataclass, field
from pathlib import Path

TAG_OK = set(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")


@dataclass
class Chunk:
    tag: str
    off: int  # payload offset in blob
    length: int
    depth: int = 0

    def data(self, blob):
        return blob[self.off:self.off + self.length]


def iter_chunks(blob, start=0, end=None, depth=0):
    """Flat walk of a chunk stream. Skips junk bytes conservatively."""
    n = len(blob) if end is None else end
    o = start
    while o + 8 <= n:
        tag = blob[o:o + 4]
        if all(c in TAG_OK for c in tag):
            (ln,) = struct.unpack_from("<I", blob, o + 4)
            if ln <= n - o - 8:
                yield Chunk(tag.decode("ascii"), o + 8, ln, depth)
                o += 8 + ln
                continue
        o += 1


CONTAINERS = {"DGCG", "ESCH"}


def walk(blob, start=0, end=None, depth=0, containers=CONTAINERS):
    """Recursive walk that descends into container chunks."""
    for c in iter_chunks(blob, start, end, depth):
        yield c
        if c.tag in containers and c.length > 8:
            yield from walk(blob, c.off, c.off + c.length, depth + 1, containers)


def unwrap(buf: bytes) -> bytes:
    """Peel ZICB (zlib) / NECB (xor 0x04) layers until we reach QVRS."""
    for _ in range(8):
        if buf[:4] == b"QVRS":
            return buf
        nxt = None
        for c in iter_chunks(buf):
            if c.tag == "ZICB":
                nxt = zlib.decompress(buf[c.off:c.off + c.length])
                break
            if c.tag == "NECB":
                nxt = bytes(b ^ 4 for b in buf[c.off:c.off + c.length])
                break
        if nxt is None:
            return buf
        buf = nxt
    return buf


def load(path) -> bytes:
    return unwrap(Path(path).read_bytes())


def guid(b: bytes) -> str:
    return str(uuid.UUID(bytes_le=b[:16])).upper()


def latin1(b: bytes) -> str:
    return b.split(b"\x00")[0].decode("latin-1", "replace")


# ---------------------------------------------------------------- channels.lst

def load_channel_types(lst_path):
    """GUID -> (type name, dll). channels.lst is the same chunk format."""
    blob = Path(lst_path).read_bytes()
    out = {}
    cur_name = cur_guid = None
    for c in iter_chunks(blob):
        d = blob[c.off:c.off + c.length]
        if c.tag == "CHTY":
            cur_name = latin1(d)
            # GUID sits after the fixed-size name field; find it by scanning
            # the tail of the record (payload is 132 bytes: name[80]+guids+ints)
            cur_guid = guid(d[80:96])
            out[cur_guid] = [cur_name, None, guid(d[96:112])]
        elif c.tag == "C1FN" and cur_guid:
            out[cur_guid][1] = latin1(d)
    return {k: tuple(v) for k, v in out.items()}
