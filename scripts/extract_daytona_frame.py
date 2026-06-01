#!/usr/bin/env python3
"""Extract a structured Daytona frame model from renderer diagnostic logs."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


RE_LIVERB = re.compile(
    r"LIVERB#(?P<num>\d+) lr=(?P<lr>[0-9A-F]+).* r6=(?P<r6>[0-9A-F]+).* "
    r"rb=\[(?P<rb>[0-9A-F]+),\+(?P<dw>\d+)\]"
)
RE_IB = re.compile(r"(?P<prefix>(?:\s*>>)*).*===IB#(?P<num>\d+) phys=(?P<phys>[0-9A-F]+) dwords=(?P<dw>\d+)")
RE_DRAWCORR = re.compile(
    r"DRAWCORR off=(?P<off>[0-9A-F]+) op=(?P<op>[0-9A-F]+) cnt=(?P<cnt>\d+) "
    r"vf=(?P<vf>\d+) tf=(?P<tf>\d+)"
)
RE_CORR_V = re.compile(
    r"\s+V(?P<idx>\d+) phys=(?P<phys>[0-9A-F]+) bytes=(?P<bytes>\d+) "
    r"endian=(?P<endian>\d+) age=(?P<age>\d+)"
)
RE_CORR_T = re.compile(
    r"\s+T(?P<idx>\d+) base=(?P<base>[0-9A-F]+) mip=(?P<mip>[0-9A-F]+) "
    r"(?P<w>\d+)x(?P<h>\d+) fmt=(?P<fmt>\d+) dim=(?P<dim>\d+) "
    r"pitch=(?P<pitch>\d+) tiled=(?P<tiled>\d+) age=(?P<age>\d+)"
)
RE_TFETCH = re.compile(
    r"TFETCH off=(?P<off>[0-9A-F]+) reg=(?P<reg>[0-9A-F]+) idx=(?P<idx>\d+) "
    r"base=(?P<base>[0-9A-F]+) mip=(?P<mip>[0-9A-F]+) "
    r"(?P<w>\d+)x(?P<h>\d+) fmt=(?P<fmt>\d+) dim=(?P<dim>\d+) "
    r"pitch=(?P<pitch>\d+) tiled=(?P<tiled>\d+) endian=(?P<endian>\d+)"
)
RE_LOADCTX = re.compile(
    r"LOADCTX phys=(?P<phys>[0-9A-F]+) words=(?P<words>\d+) "
    r"offs=(?P<offs>\d+) hits=(?P<hits>\d+) first=(?P<first>[0-9A-F]+)"
)
RE_SAMPLEF = re.compile(r"VFETCHSAMPLEF phys=(?P<phys>[0-9A-F]+) (?P<vals>.*)")


def hex_int(value: str) -> int:
    return int(value, 16)


def parse_float_list(text: str) -> list[float | str]:
    out: list[float | str] = []
    for part in text.split():
        try:
            out.append(float(part))
        except ValueError:
            out.append(part)
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", default="logs/daytona_run.log")
    parser.add_argument("-o", "--output", default="logs/daytona_frame_model.json")
    parser.add_argument("--limit-draws", type=int, default=0, help="0 keeps all draw records")
    args = parser.parse_args()

    path = Path(args.log)
    model: dict[str, object] = {
        "source_log": str(path),
        "ring_windows": [],
        "ibs": [],
        "draws": [],
        "tfetch_events": [],
        "load_context_blocks": [],
        "vertex_samples": {},
    }

    current_ib: dict[str, object] | None = None
    current_draw: dict[str, object] | None = None
    draw_limit_hit = False

    for line in path.read_text(errors="replace").splitlines():
        if m := RE_LIVERB.search(line):
            model["ring_windows"].append({
                "num": int(m["num"]),
                "lr": m["lr"],
                "r6": m["r6"],
                "rb": m["rb"],
                "dwords": int(m["dw"]),
            })
            continue

        if m := RE_IB.search(line):
            prefix = m["prefix"] or ""
            current_ib = {
                "num": int(m["num"]),
                "phys": m["phys"],
                "dwords": int(m["dw"]),
                "depth": prefix.count(">>"),
            }
            model["ibs"].append(current_ib)
            current_draw = None
            continue

        if m := RE_DRAWCORR.search(line):
            if args.limit_draws and len(model["draws"]) >= args.limit_draws:
                draw_limit_hit = True
                current_draw = None
                continue
            current_draw = {
                "ib": current_ib,
                "off": m["off"],
                "op": m["op"],
                "count": int(m["cnt"]),
                "vertex_fetch_count": int(m["vf"]),
                "texture_fetch_count": int(m["tf"]),
                "vertices": [],
                "textures": [],
            }
            model["draws"].append(current_draw)
            continue

        if current_draw is not None and (m := RE_CORR_V.search(line)):
            current_draw["vertices"].append({
                "slot": int(m["idx"]),
                "phys": m["phys"],
                "phys_int": hex_int(m["phys"]),
                "bytes": int(m["bytes"]),
                "endian": int(m["endian"]),
                "age": int(m["age"]),
            })
            continue

        if current_draw is not None and (m := RE_CORR_T.search(line)):
            current_draw["textures"].append({
                "slot": int(m["idx"]),
                "base": m["base"],
                "base_int": hex_int(m["base"]),
                "mip": m["mip"],
                "width": int(m["w"]),
                "height": int(m["h"]),
                "format": int(m["fmt"]),
                "dimension": int(m["dim"]),
                "pitch": int(m["pitch"]),
                "tiled": bool(int(m["tiled"])),
                "age": int(m["age"]),
            })
            continue

        if m := RE_TFETCH.search(line):
            model["tfetch_events"].append({
                "off": m["off"],
                "reg": m["reg"],
                "slot": int(m["idx"]),
                "base": m["base"],
                "base_int": hex_int(m["base"]),
                "mip": m["mip"],
                "mip_int": hex_int(m["mip"]),
                "width": int(m["w"]),
                "height": int(m["h"]),
                "format": int(m["fmt"]),
                "dimension": int(m["dim"]),
                "pitch": int(m["pitch"]),
                "tiled": bool(int(m["tiled"])),
                "endian": int(m["endian"]),
            })
            continue

        if m := RE_LOADCTX.search(line):
            model["load_context_blocks"].append({
                "phys": m["phys"],
                "phys_int": hex_int(m["phys"]),
                "words": int(m["words"]),
                "offset_words": int(m["offs"]),
                "hits": int(m["hits"]),
                "first_packet_offset": m["first"],
            })
            continue

        if m := RE_SAMPLEF.search(line):
            model["vertex_samples"][m["phys"]] = parse_float_list(m["vals"])

    textures: dict[tuple[object, ...], dict[str, object]] = {}
    vertex_buffers: dict[tuple[object, ...], dict[str, object]] = {}
    for draw in model["draws"]:
        draw_op = draw["op"]
        for tex in draw["textures"]:
            key = (
                tex["base"], tex["mip"], tex["width"], tex["height"],
                tex["format"], tex["dimension"], tex["pitch"], tex["tiled"],
            )
            entry = textures.setdefault(key, {
                "base": tex["base"],
                "base_int": tex["base_int"],
                "mip": tex["mip"],
                "mip_int": hex_int(tex["mip"]),
                "width": tex["width"],
                "height": tex["height"],
                "format": tex["format"],
                "dimension": tex["dimension"],
                "pitch": tex["pitch"],
                "tiled": tex["tiled"],
                "draw_uses": 0,
                "slots": {},
                "ops": {},
            })
            entry["draw_uses"] += 1
            entry["slots"][str(tex["slot"])] = entry["slots"].get(str(tex["slot"]), 0) + 1
            entry["ops"][draw_op] = entry["ops"].get(draw_op, 0) + 1

        for vertex in draw["vertices"]:
            key = (vertex["phys"], vertex["bytes"], vertex["endian"])
            entry = vertex_buffers.setdefault(key, {
                "phys": vertex["phys"],
                "phys_int": vertex["phys_int"],
                "bytes": vertex["bytes"],
                "endian": vertex["endian"],
                "draw_uses": 0,
                "slots": {},
                "ops": {},
            })
            entry["draw_uses"] += 1
            entry["slots"][str(vertex["slot"])] = entry["slots"].get(str(vertex["slot"]), 0) + 1
            entry["ops"][draw_op] = entry["ops"].get(draw_op, 0) + 1

    model["textures"] = sorted(
        textures.values(),
        key=lambda item: (-item["draw_uses"], item["base_int"], item["format"]),
    )
    model["vertex_buffers"] = sorted(
        vertex_buffers.values(),
        key=lambda item: (-item["draw_uses"], item["phys_int"], item["bytes"]),
    )

    model["counts"] = {
        "ring_windows": len(model["ring_windows"]),
        "ibs": len(model["ibs"]),
        "draws": len(model["draws"]),
        "textures": len(model["textures"]),
        "tfetch_events": len(model["tfetch_events"]),
        "vertex_buffers": len(model["vertex_buffers"]),
        "vertex_samples": len(model["vertex_samples"]),
        "load_context_blocks": len(model["load_context_blocks"]),
        "draw_limit_hit": draw_limit_hit,
    }

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(model, indent=2, sort_keys=True) + "\n")

    textured = sum(1 for d in model["draws"] if d["textures"])
    vertexed = sum(1 for d in model["draws"] if d["vertices"])
    print(f"wrote {out_path}")
    print(f"draws={len(model['draws'])} with_vertices={vertexed} with_textures={textured}")
    print(f"textures={len(model['textures'])} vertex_buffers={len(model['vertex_buffers'])}")
    print(f"ibs={len(model['ibs'])} ring_windows={len(model['ring_windows'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
