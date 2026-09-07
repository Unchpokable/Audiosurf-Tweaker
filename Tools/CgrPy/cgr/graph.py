"""Structured Quest3D channel-graph reader.

Framing (verified byte-for-byte against `unprotected - StatsCollector.cgr`,
which ships in plaintext -- no zlib, no XOR):

    QVRS u32 version          (60)
    A3DG                      <- BARE 4-byte marker, NO length field
    CGGG guid16               group guid
    CGUC u32
    CHCO u32                  channel count
    ... then per channel:
        CHIX u32              channel index
        CHID guid16           channel *type* guid -> channels.lst
        [CHES]                <- BARE marker: channel is an external/public ref
        CHIC u8
        CHNA str              channel name
        CHIT u32
        [CHES u32 len]        <- length-framed: external-source record
        CHLC u32              link-slot count
        (CHLI i32, CHUL u32, [CHRP u32])*    child index (-1 = empty slot)
        CHUP u8
        <type-specific chunks: FLVA f32, STVA str, ATRS/ATRD rows, LUSL/LUSC...>

`CHES` is genuinely overloaded (bare marker AND length-framed chunk), so the
reader validates every framing choice with a one-token lookahead against a
whitelist of known tags.
"""

import struct
from dataclasses import dataclass, field

from .core import guid, latin1

# Every tag observed across the Audiosurf .cgr corpus.
KNOWN_TAGS = set("""
QVRS A3DG CGGG CGUC CHCO CHIX CHID CHIC CHNA CHIT CHLC CHLI CHUL CHRP CHUP
CHES CHTM CHSU CHVE CHDE CHAU
FLVA STVA STWA STLR STOT STNW VAVA
ATRS ATRD ATCT ATCN ATCW ATUI ATRC ATCI ATNC ATNR ATVA
OPCN OPFL OPRC OPTY
VECT VEOT VEUN VEFF VECF VECV VEVA
SCHI CWGU AIVS AVFR TIST TISC
LUSL LUSC LUWS
FLCS FLSC FLCN FLIP FLIN FLLC
DGCG ESCH TENT TECM TEAM TEOF
RTEF RTBS RHDR RHCS RHBB RHXB
TXNA TXFN TXFL TXSI TXMI TXTY
SONA SOFN SOFL MONA MOFN
EXPR EXST EXNC EXCN
IFCO IFVA CSVA CSIX
BUFF BUSI BUNA
""".split())

BARE_TAGS = {"A3DG", "CHES", "STNW"}  # tags that may appear with no length field


_TAGCH = frozenset(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")


def _is_tag(blob, o):
    """Structural test: 4 bytes of [A-Z0-9_].

    A fixed whitelist was tried first and rejected -- the corpus keeps yielding
    new type-specific tags (ATTG/ACCS/ENV1/CEXI/COVA/TCBS/...). Structural
    matching is safe *as long as sync is never lost*: payloads are skipped
    wholesale, so the test only ever runs at a known-good chunk boundary.
    """
    if o + 4 > len(blob):
        return False
    return all(c in _TAGCH for c in blob[o:o + 4])


def iter_chunks2(blob, start=0, end=None, strict=True):
    """Sequential chunk walk with one-token lookahead validation.

    Yields (tag, payload_offset, length, payload). length == -1 marks a bare
    marker. Never silently resyncs in strict mode: an unparsable position is
    reported via the 'BAD!' pseudo-tag so callers can see coverage loss.
    """
    n = len(blob) if end is None else end
    o = start
    while o + 4 <= n:
        if not _is_tag(blob, o):
            if strict:
                yield "BAD!", o, -1, b""
            o += 1
            continue
        t = blob[o:o + 4].decode("ascii")

        framed = None
        if o + 8 <= n:
            (ln,) = struct.unpack_from("<I", blob, o + 4)
            nxt = o + 8 + ln
            if ln <= n - o - 8 and (nxt == n or _is_tag(blob, nxt)):
                framed = (ln, nxt)

        if t in BARE_TAGS and _is_tag(blob, o + 4):
            # bare framing validates -> prefer it (a real length field would
            # have to spell a known tag, which never happens in practice)
            yield t, o + 4, -1, b""
            o += 4
            continue

        if framed is not None:
            ln, nxt = framed
            yield t, o + 8, ln, blob[o + 8:o + 8 + ln]
            o = nxt
            continue

        if t in BARE_TAGS:
            yield t, o + 4, -1, b""
            o += 4
            continue

        if strict:
            yield "BAD!", o, -1, b""
        o += 1


@dataclass
class Link:
    child: int
    ul: int = 0
    rp: int = -1


@dataclass
class Channel:
    index: int = -1
    type_guid: str = ""
    type_name: str = "?"
    name: str = ""
    external: bool = False        # had the bare CHES marker
    ext_record: bytes = b""       # length-framed CHES payload
    links: list = field(default_factory=list)
    chunks: list = field(default_factory=list)  # (tag, bytes)
    group: str = ""

    def tag(self, t):
        for tt, d in self.chunks:
            if tt == t:
                return d
        return None

    def tags(self, t):
        return [d for tt, d in self.chunks if tt == t]

    @property
    def floats(self):
        return [struct.unpack("<f", d)[0]
                for tt, d in self.chunks if tt in ("FLVA", "VAVA") and len(d) == 4]

    @property
    def value(self):
        f = self.floats
        return f[0] if f else None

    @property
    def text(self):
        for tt, d in self.chunks:
            if tt in ("STVA", "TXVA"):
                return latin1(d)
        return None

    @property
    def children(self):
        return [l.child for l in self.links]

    @property
    def kids(self):
        return [l.child for l in self.links if l.child >= 0]

    def __repr__(self):
        return f"<#{self.index} {self.type_name} '{self.name}'>"


HEADER = {"CHIX", "CHID", "CHIC", "CHNA", "CHIT", "CHLC", "CHLI", "CHUL", "CHRP", "CHUP"}


def parse_graph(blob, types=None, group=""):
    """Return (channels_by_index, meta)."""
    channels, order = {}, []
    meta = {"bad": 0, "bad_bytes": 0}
    cur = None
    pending_ext = False

    def flush():
        nonlocal cur
        if cur is not None:
            channels[cur.index] = cur
            order.append(cur.index)
            cur = None

    for t, off, ln, d in iter_chunks2(blob):
        if t == "BAD!":
            meta["bad_bytes"] += 1
            continue
        if t == "CHIX":
            flush()
            cur = Channel(index=struct.unpack("<i", d)[0], group=group)
            continue
        if cur is None:
            if t == "CHCO" and ln == 4:
                meta["channel_count"] = struct.unpack("<I", d)[0]
            elif t == "CGGG" and ln == 16:
                meta["group_guid"] = guid(d)
            elif t == "QVRS" and ln == 4:
                meta["version"] = struct.unpack("<I", d)[0]
            continue
        if t == "CHID":
            cur.type_guid = guid(d)
        elif t == "CHNA":
            cur.name = latin1(d)
        elif t == "CHES":
            if ln == -1:
                cur.external = True
            else:
                cur.ext_record = d
        elif t == "CHLI":
            cur.links.append(Link(struct.unpack("<i", d)[0]))
        elif t == "CHUL":
            if cur.links:
                cur.links[-1].ul = struct.unpack("<I", d)[0]
        elif t == "CHRP":
            if cur.links:
                cur.links[-1].rp = struct.unpack("<I", d)[0]
        elif t in HEADER:
            pass
        else:
            cur.chunks.append((t, d))
    flush()

    if types:
        for c in channels.values():
            c.type_name = types.get(c.type_guid, ("?",))[0]
    meta["order"] = order
    return channels, meta
