r"""Locating and replacing geometry inside the L2 chunk stream of a .cgr.

Why this works on byte spans instead of on parsed channels
----------------------------------------------------------
A mesh is *not* reliably one-per-channel. `Render/FixedChainspans.cgr` channel
#72 is an `Array Table` named "Chainspans Table" whose record carries ten
complete `Tune_Wall` mesh blobs inline, each introduced by its own `CHNA` and
none by a `CHIX`. Keying meshes by channel index silently collapses those ten
into one. So everything here is keyed by **stream ordinal**: meshes are
numbered in the order their `VRCO` appears in the L2 stream.

A mesh span is the maximal run of consecutive geometry chunks starting at a
`VRCO`. That is exactly how `Aco_DX8_ObjectDataChannel::LoadChannel` consumes
them, and it holds for every one of the 933 mesh blobs in the shipped corpus.

Splicing is safe because the L2 stream is flat and self-delimiting -- TAG +
u32 len + payload, no absolute offsets anywhere -- so a replacement of a
different size just shifts everything downstream. `CHCO` is unaffected: no
channel is added or removed.

    core, env = container.unwrap_file(path)
    core = replace_mesh(core, ordinal, mesh)
    Path(out).write_bytes(container.rewrap(core, env))
"""

import struct
from dataclasses import dataclass

from .graph import iter_chunks2
from .mesh import GEOM_TAGS, Mesh


@dataclass
class MeshRef:
    """Where one mesh blob lives in an L2 stream."""
    ordinal: int          # 0-based, in stream order
    start: int            # byte offset of the VRCO chunk header
    end: int              # byte offset just past the last geometry chunk
    channel_index: int    # owning CHIX record, -1 if none seen yet
    channel_name: str     # CHNA most recently seen before the VRCO
    channel_type: str = ""
    tags: tuple = ()

    @property
    def size(self):
        return self.end - self.start


def _emit(chunks):
    return b"".join(t.encode("ascii") + struct.pack("<I", len(d)) + d for t, d in chunks)


def scan_meshes(core):
    """-> [MeshRef] for every mesh blob in the stream, in order."""
    refs = []
    cur_idx, cur_name = -1, ""
    open_ref = None
    tags = []

    def close(end):
        nonlocal open_ref, tags
        if open_ref is not None:
            open_ref.end = end
            open_ref.tags = tuple(tags)
            refs.append(open_ref)
            open_ref, tags = None, []

    for tag, off, ln, d in iter_chunks2(core):
        if tag == "BAD!":
            continue
        start = off - (4 if ln == -1 else 8)
        end = off + (0 if ln == -1 else ln)

        if tag == "VRCO":
            close(start)
            open_ref = MeshRef(len(refs), start, end, cur_idx, cur_name)
            tags = [tag]
            continue
        if open_ref is not None:
            if tag in GEOM_TAGS and start == open_ref.end:
                open_ref.end = end
                tags.append(tag)
                continue
            close(open_ref.end)

        if tag == "CHIX":
            cur_idx = struct.unpack("<i", d)[0]
            cur_name = ""
        elif tag == "CHNA":
            cur_name = d.split(b"\0")[0].decode("latin-1", "replace")
    close(open_ref.end if open_ref else 0)
    return refs


def read_mesh(core, ref):
    """Decode the mesh blob at `ref` straight from the stream.

    Independent of the graph parser, so it still works in the two groups whose
    channel framing is damaged (`XX_StartHere.cgr`,
    `Render/Render_IndustrialTunnel.cgr`).
    """
    chunks = []
    for tag, off, ln, d in iter_chunks2(core, ref.start, ref.end):
        if tag != "BAD!":
            chunks.append((tag, d))

    class _Shim:
        def __init__(self, ch):
            self.chunks = ch
            self.name = ref.channel_name
            self.index = ref.channel_index

        def tag(self, t):
            for tt, dd in self.chunks:
                if tt == t:
                    return dd
            return None

    return Mesh.from_channel(_Shim(chunks))


def replace_mesh(core, ordinal, mesh):
    """Return a new L2 stream with mesh blob #`ordinal` replaced."""
    refs = scan_meshes(core)
    if not 0 <= ordinal < len(refs):
        raise IndexError(f"mesh ordinal {ordinal} out of range (stream has {len(refs)})")
    r = refs[ordinal]
    return core[:r.start] + _emit(mesh.to_chunks()) + core[r.end:]


def replace_meshes(core, by_ordinal):
    """Splice several at once; applied back to front so offsets stay valid."""
    refs = scan_meshes(core)
    bad = [o for o in by_ordinal if not 0 <= o < len(refs)]
    if bad:
        raise IndexError(f"mesh ordinal(s) {bad} out of range (stream has {len(refs)})")
    out = core
    for o in sorted(by_ordinal, reverse=True):
        r = refs[o]
        out = out[:r.start] + _emit(by_ordinal[o].to_chunks()) + out[r.end:]
    return out
