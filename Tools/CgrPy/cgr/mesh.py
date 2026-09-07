r"""Geometry stored in `3D ObjectData` channels (Aco_DX8_ObjectDataChannel).

Chunk layout, confirmed against `LoadChannel` @ RVA 0x4140 in
`channels/21A8923D-B908-4104-AE88-B6718D8A8678.dll`, and validated on all 808
meshes in the shipped corpus with zero mismatches:

    VRCO u32          vertex count            -> this+0x98
    VRFL u32          D3D FVF                 -> this+0xb8
    VPPI f32[3*n]     positions               -> this+0x9c   (gated by FVF & 0x002 XYZ)
    VNNI f32[3*n]     normals                 -> this+0xa0   (gated by FVF & 0x010 NORMAL)
    VTD0 f32[2*n]     texcoord set 0          -> this+0xa4   (gated by FVF & 0x100)
    VTD1 f32[2*n]     texcoord set 1          -> this+0xa8   (gated by FVF & 0x200)
    VTD2 f32[2*n]     texcoord set 2          -> this+0xac   (gated by FVF & 0x300)
    VTO0/1/2  24B     VertTUVOffset per set   -> this+0xc4 + set*0x18
    POCO u32          INDEX count (not tris)  -> this+0x94
    PODA u16[POCO]    16-bit indices          -> this+0x8c
    POTY u32          D3DPRIMITIVETYPE        -> this+0xc0  (always 4 = TRIANGLELIST)
    PONM u32[3]       per-stage texture mapping mode  -> this+0xc8
    POTT u32[3]       per-stage texture transform     -> this+0x194

Tags the loader accepts but the shipped corpus never uses: `VPDA`/`VNDA`/`VTDA`/
`VTOP` (older revision spellings), `PO32` (u32[POCO] 32-bit indices, selected
when vertex count >= 0x10000), `VCDA`/`VCNC` (u32 vertex colours, FVF 0x040).

Two facts that matter when writing meshes back:

* `CreateVertexBuffer` **recomputes the FVF from which arrays are non-NULL**
  and ignores the loaded VRFL. VRFL only decides which arrays LoadChannel
  allocates, so it must still describe the chunks actually present.
* The engine validates every index against the vertex count and refuses to
  build the buffer ("Invalid index data") rather than crashing.
"""

import struct

FVF_XYZ = 0x002
FVF_NORMAL = 0x010
FVF_DIFFUSE = 0x040
D3DPT_TRIANGLELIST = 4


def _f32(d):
    return list(struct.unpack(f"<{len(d) // 4}f", d))


class Mesh:
    """One `3D ObjectData` channel, decoded."""

    __slots__ = ("name", "group", "index", "fvf", "positions", "normals", "colors",
                 "uvs", "uv_offsets", "indices", "poly_type", "ponm", "pott")

    def __init__(self, name="", group="", index=-1):
        self.name = name
        self.group = group
        self.index = index
        self.fvf = FVF_XYZ | FVF_NORMAL
        self.positions = []          # [(x,y,z)]
        self.normals = []            # [(x,y,z)]
        self.colors = []             # [u32 D3DCOLOR] from VCNC; never declared in VRFL
        self.uvs = []                # up to 3 lists of [(u,v)]
        self.uv_offsets = []         # up to 3 tuples of 6 floats (VTOn)
        self.indices = []            # flat, 3 per triangle
        self.poly_type = D3DPT_TRIANGLELIST
        self.ponm = (7, 7, 7)
        self.pott = (0, 0, 0)

    # ------------------------------------------------------------------ read
    @classmethod
    def from_channel(cls, chan, group=""):
        """Decode a cgr.graph.Channel of type '3D ObjectData'. None if it has no VRCO."""
        if chan.tag("VRCO") is None:
            return None
        m = cls(name=chan.name, group=group, index=chan.index)
        n = struct.unpack("<I", chan.tag("VRCO"))[0]
        vrfl = chan.tag("VRFL")
        m.fvf = struct.unpack("<I", vrfl)[0] if vrfl else 0

        if chan.tag("VPPI") is None:
            # Declaration-only blob: VRCO/VRFL/POCO/POTY present, no vertex arrays.
            # 29 of these ship, in HLSLObject / CustomGeometry channels that build
            # their geometry at runtime. `has_geometry` is False for them.
            poty = chan.tag("POTY")
            if poty:
                m.poly_type = struct.unpack("<I", poty)[0]
            return m

        p = _f32(chan.tag("VPPI"))
        m.positions = [tuple(p[i:i + 3]) for i in range(0, 3 * n, 3)]
        nn = chan.tag("VNNI")
        if nn:
            q = _f32(nn)
            m.normals = [tuple(q[i:i + 3]) for i in range(0, 3 * n, 3)]

        for s in range(3):
            td = chan.tag(f"VTD{s}")
            if td is None:
                break
            t = _f32(td)
            m.uvs.append([tuple(t[i:i + 2]) for i in range(0, 2 * n, 2)])
            to = chan.tag(f"VTO{s}")
            m.uv_offsets.append(tuple(_f32(to)) if to else (0.0,) * 6)

        vc = chan.tag("VCNC")
        if vc:
            m.colors = list(struct.unpack(f"<{len(vc) // 4}I", vc))

        poco = chan.tag("POCO")
        if poco is None:
            return m
        c = struct.unpack("<I", poco)[0]
        pd = chan.tag("PODA")
        m.indices = list(struct.unpack(f"<{c}H", pd)) if pd else []
        m.poly_type = struct.unpack("<I", chan.tag("POTY"))[0]
        if chan.tag("PONM"):
            m.ponm = struct.unpack("<3I", chan.tag("PONM"))
        if chan.tag("POTT"):
            m.pott = struct.unpack("<3I", chan.tag("POTT"))
        return m

    # --------------------------------------------------------------- summary
    @property
    def vertex_count(self):
        return len(self.positions)

    @property
    def triangle_count(self):
        return len(self.indices) // 3

    @property
    def uv_sets(self):
        return len(self.uvs)

    @property
    def has_geometry(self):
        """False for declaration-only blobs (format declared, arrays generated at runtime)."""
        return bool(self.positions)

    def bounds(self):
        if not self.positions:
            return (0, 0, 0), (0, 0, 0)
        xs, ys, zs = zip(*self.positions)
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def __repr__(self):
        return (f"<Mesh {self.name!r} {self.vertex_count}v {self.triangle_count}t "
                f"uv{self.uv_sets} fvf=0x{self.fvf:x}>")

    # ------------------------------------------------------------ OBJ export
    def to_obj(self, flip_v=True, name=None):
        """Wavefront OBJ text.

        D3D texture space puts V at the top, OBJ puts it at the bottom, so V is
        flipped by default. Winding is left as-is: the game renders with its own
        cull mode and round-tripping an untouched mesh must be a no-op.
        """
        o = [f"# {self.group}#{self.index} {self.name}",
             f"# {self.vertex_count} verts, {self.triangle_count} tris, "
             f"{self.uv_sets} uv set(s), FVF 0x{self.fvf:03x}",
             f"o {name or self.name or f'mesh_{self.index}'}"]
        for x, y, z in self.positions:
            o.append(f"v {x:.6g} {y:.6g} {z:.6g}")
        if self.uvs:
            for u, v in self.uvs[0]:
                o.append(f"vt {u:.6g} {(1.0 - v) if flip_v else v:.6g}")
        for x, y, z in self.normals:
            o.append(f"vn {x:.6g} {y:.6g} {z:.6g}")
        has_t, has_n = bool(self.uvs), bool(self.normals)
        for i in range(0, len(self.indices) - 2, 3):
            f = []
            for k in range(3):
                j = self.indices[i + k] + 1
                if has_t and has_n:
                    f.append(f"{j}/{j}/{j}")
                elif has_t:
                    f.append(f"{j}/{j}")
                elif has_n:
                    f.append(f"{j}//{j}")
                else:
                    f.append(str(j))
            o.append("f " + " ".join(f))
        return "\n".join(o) + "\n"

    # ------------------------------------------------------------ OBJ import
    @classmethod
    def from_obj(cls, text, flip_v=True, name=""):
        """Parse a triangulated OBJ into a Mesh.

        OBJ indexes v/vt/vn independently; D3D needs one index per vertex, so
        every distinct v/vt/vn triple becomes its own vertex. Faces with more
        than three corners are fan-triangulated.
        """
        V, T, N = [], [], []
        remap, verts, idx = {}, [], []

        def pick(s, arr):
            if not s:
                return None
            i = int(s)
            return arr[i - 1] if i > 0 else arr[i]

        for line in text.splitlines():
            line = line.strip()
            if not line or line[0] == "#":
                continue
            w = line.split()
            k = w[0]
            if k == "v":
                V.append(tuple(float(x) for x in w[1:4]))
            elif k == "vt":
                u = float(w[1])
                v = float(w[2]) if len(w) > 2 else 0.0
                T.append((u, (1.0 - v) if flip_v else v))
            elif k == "vn":
                N.append(tuple(float(x) for x in w[1:4]))
            elif k == "f":
                corners = []
                for tok in w[1:]:
                    if tok not in remap:
                        parts = (tok.split("/") + ["", ""])[:3]
                        remap[tok] = len(verts)
                        verts.append((pick(parts[0], V), pick(parts[1], T), pick(parts[2], N)))
                    corners.append(remap[tok])
                for i in range(1, len(corners) - 1):
                    idx += [corners[0], corners[i], corners[i + 1]]

        m = cls(name=name)
        m.positions = [v[0] or (0.0, 0.0, 0.0) for v in verts]
        if any(v[2] for v in verts):
            m.normals = [v[2] or (0.0, 1.0, 0.0) for v in verts]
        if any(v[1] for v in verts):
            m.uvs = [[v[1] or (0.0, 0.0) for v in verts]]
            m.uv_offsets = [(0.0, 0.0, 0.0, 0.0, 1.0, 1.0)]
        m.indices = idx
        m.recompute_fvf()
        return m

    def recompute_fvf(self):
        """Set VRFL to describe the arrays actually present.

        Deliberately does NOT set `FVF_DIFFUSE` even when `colors` is populated:
        the 70 shipped meshes carrying `VCNC` all declare VRFL without 0x040, so
        `LoadChannel` clearly reads the colour array ungated, and synthesising the
        bit would be a change of unverified effect. `CreateVertexBuffer` adds it
        at render time anyway, from the array pointer rather than from VRFL.

        Bits outside the XYZ/NORMAL/TEXn mask are preserved.
        """
        f = self.fvf & ~(FVF_XYZ | FVF_NORMAL | 0xF00)
        if self.positions:
            f |= FVF_XYZ
        if self.normals:
            f |= FVF_NORMAL
        if self.uvs:
            f |= (len(self.uvs) & 0xF) << 8
        self.fvf = f
        return f

    # ------------------------------------------------------------ serialise
    def validate(self):
        """[] when the mesh is something the engine will accept."""
        e = []
        n = len(self.positions)
        if n == 0:
            e.append("no vertices")
        if n > 0xFFFF:
            e.append(f"{n} vertices exceeds the 16-bit PODA index range "
                     "(engine supports PO32 above 0x10000, this writer does not)")
        if self.normals and len(self.normals) != n:
            e.append(f"normal count {len(self.normals)} != vertex count {n}")
        if self.colors and len(self.colors) != n:
            e.append(f"color count {len(self.colors)} != vertex count {n}")
        for s, uv in enumerate(self.uvs):
            if len(uv) != n:
                e.append(f"uv set {s} count {len(uv)} != vertex count {n}")
        if len(self.uvs) > 3:
            e.append(f"{len(self.uvs)} uv sets, engine supports 3")
        if len(self.indices) % 3:
            e.append(f"index count {len(self.indices)} is not a multiple of 3")
        if self.indices and max(self.indices) >= n:
            e.append(f"index {max(self.indices)} out of range for {n} vertices")
        return e

    def to_chunks(self):
        """[(tag, payload)] in the order LoadChannel reads them."""
        bad = self.validate()
        if bad:
            raise ValueError("; ".join(bad))
        self.recompute_fvf()
        n = len(self.positions)
        out = [("VRCO", struct.pack("<I", n)),
               ("VRFL", struct.pack("<I", self.fvf)),
               ("VPPI", b"".join(struct.pack("<3f", *p) for p in self.positions))]
        if self.normals:
            out.append(("VNNI", b"".join(struct.pack("<3f", *v) for v in self.normals)))
        for s, uv in enumerate(self.uvs):
            off = self.uv_offsets[s] if s < len(self.uv_offsets) else (0.0, 0.0, 0.0, 0.0, 1.0, 1.0)
            out.append((f"VTD{s}", b"".join(struct.pack("<2f", *t) for t in uv)))
            out.append((f"VTO{s}", struct.pack("<6f", *off)))
        if self.colors:
            out.append(("VCNC", struct.pack(f"<{len(self.colors)}I", *self.colors)))
        out += [("POCO", struct.pack("<I", len(self.indices))),
                ("PODA", struct.pack(f"<{len(self.indices)}H", *self.indices)),
                ("POTY", struct.pack("<I", self.poly_type)),
                ("PONM", struct.pack("<3I", *self.ponm)),
                ("POTT", struct.pack("<3I", *self.pott))]
        return out


GEOM_TAGS = {"VRCO", "VRFL", "VPPI", "VNNI", "VPDA", "VNDA", "VCDA", "VCNC",
             "VTD0", "VTD1", "VTD2", "VTO0", "VTO1", "VTO2", "VTDA", "VTOP",
             "POCO", "PODA", "PO32", "POTY", "POTM", "PONM", "POTT"}


def iter_meshes(project):
    """Every decodable mesh in a Project, in (group, channel index) order."""
    for rel in sorted(project.groups):
        g = project.groups[rel]
        for ch in sorted(g.channels.values(), key=lambda c: c.index):
            if ch.type_name != "3D ObjectData":
                continue
            m = Mesh.from_channel(ch, group=rel)
            if m is not None:
                yield m
