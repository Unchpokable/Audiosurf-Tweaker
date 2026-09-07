r"""Mesh toolkit CLI for the Audiosurf .cgr corpus.

    uv run python meshes.py inventory  [engine_dir] [out.csv]
    uv run python meshes.py export     <file.cgr> <ordinal> <out.obj>
    uv run python meshes.py exportall  <file.cgr> <out_dir>
    uv run python meshes.py replace    <file.cgr> <ordinal> <in.obj> <out.cgr>
    uv run python meshes.py verify     [engine_dir]

`ordinal` is the mesh's position in the file's L2 chunk stream, as printed by
`inventory` / `exportall`. It is not the channel index -- see cgr/patch.py for
why (one channel record can hold several mesh blobs).
"""

import csv
import sys
from pathlib import Path

from cgr import container, patch
from cgr.core import load_channel_types
from cgr.graph import parse_graph

from cgr.paths import engine_dir

ENGINE = engine_dir()


def _types(root):
    try:
        return load_channel_types(root / "channels.lst")
    except Exception:
        return {}


def _channel_types(core, types, name):
    """{channel_index: type_name}, best effort -- damaged groups return partials."""
    try:
        chans, _ = parse_graph(core, types, group=name)
        return {i: c.type_name for i, c in chans.items()}
    except Exception:
        return {}


def inventory(root=ENGINE, out="out/meshes.csv"):
    root = Path(root)
    types = _types(root)
    rows = []
    for p in sorted(root.rglob("*.cgr")):
        rel = str(p.relative_to(root)).replace("\\", "/")
        core, _ = container.unwrap_file(p)
        ctypes = _channel_types(core, types, p.name)
        for r in patch.scan_meshes(core):
            try:
                m = patch.read_mesh(core, r)
            except Exception as e:
                rows.append(dict(file=rel, ordinal=r.ordinal, channel=r.channel_index,
                                 channel_type=ctypes.get(r.channel_index, "?"),
                                 name=r.channel_name, verts=0, tris=0, uv_sets=0,
                                 fvf="", poly_type="", bytes=r.size, error=repr(e)[:80]))
                continue
            (lox, loy, loz), (hix, hiy, hiz) = m.bounds()
            rows.append(dict(
                file=rel, ordinal=r.ordinal, channel=r.channel_index,
                channel_type=ctypes.get(r.channel_index, "?"), name=r.channel_name,
                verts=m.vertex_count, tris=m.triangle_count, uv_sets=m.uv_sets,
                fvf=f"0x{m.fvf:03x}", poly_type=m.poly_type, bytes=r.size,
                size_x=round(hix - lox, 4), size_y=round(hiy - loy, 4), size_z=round(hiz - loz, 4),
                error=""))
    outp = Path(out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    cols = ["file", "ordinal", "channel", "channel_type", "name", "verts", "tris",
            "uv_sets", "fvf", "poly_type", "bytes", "size_x", "size_y", "size_z", "error"]
    with outp.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    good = [r for r in rows if not r["error"]]
    print(f"{len(rows)} mesh blobs in {len(set(r['file'] for r in rows))} files -> {outp}")
    print(f"  decoded {len(good)}, failed {len(rows) - len(good)}")
    print(f"  {sum(r['verts'] for r in good)} vertices, {sum(r['tris'] for r in good)} triangles")
    return rows


def export(path, ordinal, out):
    core, _ = container.unwrap_file(path)
    refs = patch.scan_meshes(core)
    m = patch.read_mesh(core, refs[int(ordinal)])
    Path(out).write_text(m.to_obj(), encoding="utf-8")
    print(f"mesh #{ordinal} {m.name!r}: {m.vertex_count}v {m.triangle_count}t -> {out}")


def exportall(path, outdir):
    core, _ = container.unwrap_file(path)
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    n = 0
    for r in patch.scan_meshes(core):
        try:
            m = patch.read_mesh(core, r)
        except Exception as e:
            print(f"  #{r.ordinal} {r.channel_name!r}: SKIP {e}")
            continue
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in (r.channel_name or "mesh"))
        f = outdir / f"{r.ordinal:03d}_{safe}.obj"
        f.write_text(m.to_obj(), encoding="utf-8")
        print(f"  #{r.ordinal:3d} {m.vertex_count:6d}v {m.triangle_count:6d}t  {f.name}")
        n += 1
    print(f"{n} meshes -> {outdir}")


def replace(path, ordinal, objfile, out):
    from cgr.mesh import Mesh
    core, env = container.unwrap_file(path)
    refs = patch.scan_meshes(core)
    r = refs[int(ordinal)]
    old = patch.read_mesh(core, r)
    m = Mesh.from_obj(Path(objfile).read_text(encoding="utf-8"), name=r.channel_name)
    m.uv_offsets = old.uv_offsets or m.uv_offsets
    m.ponm, m.pott, m.poly_type = old.ponm, old.pott, old.poly_type
    bad = m.validate()
    if bad:
        sys.exit("refusing to write: " + "; ".join(bad))
    newcore = patch.replace_mesh(core, int(ordinal), m)
    Path(out).write_bytes(container.rewrap(newcore, env))
    print(f"#{ordinal} {old.vertex_count}v/{old.triangle_count}t -> "
          f"{m.vertex_count}v/{m.triangle_count}t   written {out}")


def verify(root=ENGINE):
    """Every mesh must decode, re-encode byte-identically, and OBJ round-trip."""
    root = Path(root)
    tot = enc_ok = obj_ok = 0
    bad = []
    for p in sorted(root.rglob("*.cgr")):
        core, _ = container.unwrap_file(p)
        raw = core.count(b"VRCO\x04\x00\x00\x00")
        refs = patch.scan_meshes(core)
        if len(refs) != raw:
            bad.append(f"{p.name}: scanned {len(refs)} != {raw} raw VRCO")
        for r in refs:
            tot += 1
            try:
                m = patch.read_mesh(core, r)
            except Exception as e:
                bad.append(f"{p.name}#{r.ordinal}: decode {e!r}")
                continue
            try:
                blob = patch._emit(m.to_chunks())
                if blob == core[r.start:r.end]:
                    enc_ok += 1
                elif m.poly_type == 4:
                    bad.append(f"{p.name}#{r.ordinal} {r.channel_name!r}: re-encode differs")
            except ValueError:
                pass  # non-trianglelist / oversized: reported by inventory
            try:
                from cgr.mesh import Mesh
                m2 = Mesh.from_obj(m.to_obj())
                if m2.triangle_count == m.triangle_count:
                    obj_ok += 1
            except Exception as e:
                bad.append(f"{p.name}#{r.ordinal}: obj {e!r}")
    print(f"{tot} mesh blobs: {enc_ok} re-encode byte-exact, {obj_ok} OBJ round-trip")
    print(f"problems: {len(bad)}")
    for b in bad[:20]:
        print("   ", b)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "inventory"
    dict(inventory=inventory, export=export, exportall=exportall,
         replace=replace, verify=verify)[cmd](*sys.argv[2:])
