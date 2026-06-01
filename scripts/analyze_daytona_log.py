#!/usr/bin/env python3
"""Summarize Daytona renderer diagnostic logs."""

from __future__ import annotations

import argparse
import collections
import re
from pathlib import Path


RE_IB = re.compile(r"===IB#(?P<num>\d+) phys=(?P<phys>[0-9A-F]+) dwords=(?P<dw>\d+)")
RE_LIVERB = re.compile(r"LIVERB#(?P<num>\d+) lr=(?P<lr>[0-9A-F]+).* r6=(?P<r6>[0-9A-F]+).* rb=\[(?P<rb>[0-9A-F]+),\+(?P<dw>\d+)\]")
RE_RECT = re.compile(r"DRAWSUM_RECT x0=(?P<x0>\d+) y0=(?P<y0>\d+) x1=(?P<x1>\d+) y1=(?P<y1>\d+)")
RE_STRIP = re.compile(r"DRAWSUM_STRIP_SETUP w0=(?P<w0>[0-9A-F]+) w1=(?P<w1>[0-9A-F]+) .* w2f=(?P<w2f>[-0-9.]+)")
RE_TILE = re.compile(r"DRAWSUM_TILE_KEY key=(?P<key>[0-9A-F]+) w0=(?P<w0>[0-9A-F]+)")
RE_DRAW2 = re.compile(r"DRAWSUM_DRAW2 init=(?P<init>[0-9A-F]+) prim=(?P<prim>\d+) sel=(?P<sel>\d+) idx=(?P<idx>\d+)")
RE_FETCH = re.compile(r"FETCHSAMPLEF phys=(?P<phys>[0-9A-F]+) (?P<vals>.*)")
RE_VFETCH = re.compile(
    r"VFETCH off=(?P<off>[0-9A-F]+) reg=(?P<reg>[0-9A-F]+) idx=(?P<idx>\d+) "
    r"phys=(?P<phys>[0-9A-F]+) bytes=(?P<bytes>\d+) endian=(?P<endian>\d+)"
)
RE_TFETCH = re.compile(
    r"TFETCH off=(?P<off>[0-9A-F]+) reg=(?P<reg>[0-9A-F]+) idx=(?P<idx>\d+) "
    r"base=(?P<base>[0-9A-F]+) mip=(?P<mip>[0-9A-F]+) (?P<w>\d+)x(?P<h>\d+) "
    r"fmt=(?P<fmt>\d+) dim=(?P<dim>\d+) pitch=(?P<pitch>\d+) tiled=(?P<tiled>\d+) "
    r"endian=(?P<endian>\d+) raw=(?P<raw>.*)"
)
RE_DRAWCORR = re.compile(r"DRAWCORR off=(?P<off>[0-9A-F]+) op=(?P<op>[0-9A-F]+) cnt=(?P<cnt>\d+) vf=(?P<vf>\d+) tf=(?P<tf>\d+)")
RE_CORR_V = re.compile(r"\s+V(?P<idx>\d+) phys=(?P<phys>[0-9A-F]+) bytes=(?P<bytes>\d+) endian=(?P<endian>\d+) age=(?P<age>\d+)")
RE_CORR_T = re.compile(
    r"\s+T(?P<idx>\d+) base=(?P<base>[0-9A-F]+) mip=(?P<mip>[0-9A-F]+) "
    r"(?P<w>\d+)x(?P<h>\d+) fmt=(?P<fmt>\d+) dim=(?P<dim>\d+) "
    r"pitch=(?P<pitch>\d+) tiled=(?P<tiled>\d+) age=(?P<age>\d+)"
)
RE_LOADCTX = re.compile(r"LOADCTX phys=(?P<phys>[0-9A-F]+) words=(?P<words>\d+) offs=(?P<offs>\d+)")
RE_STATE = re.compile(r"STATESUM base=(?P<base>[0-9A-F]+) cnt=(?P<cnt>\d+) hits=(?P<hits>\d+)")


def first_items(counter: collections.Counter[str], limit: int) -> list[tuple[str, int]]:
    return sorted(counter.items(), key=lambda kv: (-kv[1], kv[0]))[:limit]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", default="logs/daytona_run.log")
    parser.add_argument("--limit", type=int, default=12)
    args = parser.parse_args()

    path = Path(args.log)
    lines = path.read_text(errors="replace").splitlines()

    live = []
    ibs = []
    rects: collections.Counter[str] = collections.Counter()
    strips: collections.Counter[str] = collections.Counter()
    tiles: collections.Counter[str] = collections.Counter()
    draw2: collections.Counter[str] = collections.Counter()
    fetches: collections.Counter[str] = collections.Counter()
    vfetches: collections.Counter[str] = collections.Counter()
    tfetches: collections.Counter[str] = collections.Counter()
    drawcorr: collections.Counter[str] = collections.Counter()
    drawcorr_v: collections.Counter[str] = collections.Counter()
    drawcorr_t: collections.Counter[str] = collections.Counter()
    loadctx: collections.Counter[str] = collections.Counter()
    states: collections.Counter[str] = collections.Counter()
    flags: collections.Counter[str] = collections.Counter()
    corr_key = ""

    for line in lines:
        if "UNKNOWN_TYPE" in line:
            flags["UNKNOWN_TYPE"] += 1
        if "TRUNCATED" in line:
            flags["TRUNCATED"] += 1
        if "2D rect?" in line:
            flags["OLD_RECT_GUESS"] += 1

        if m := RE_LIVERB.search(line):
            live.append((m["num"], m["lr"], m["r6"], m["rb"], m["dw"]))
        if m := RE_IB.search(line):
            ibs.append((m["num"], m["phys"], int(m["dw"])))
        if m := RE_RECT.search(line):
            rects[f"{m['x0']},{m['y0']}->{m['x1']},{m['y1']}"] += 1
        if m := RE_STRIP.search(line):
            strips[f"w0={m['w0']} w1={m['w1']} y={m['w2f']}"] += 1
        if m := RE_TILE.search(line):
            tiles[f"{m['key']} w0={m['w0']}"] += 1
        if m := RE_DRAW2.search(line):
            draw2[f"init={m['init']} prim={m['prim']} sel={m['sel']} idx={m['idx']}"] += 1
        if m := RE_FETCH.search(line):
            vals = " ".join(m["vals"].split()[:8])
            fetches[f"{m['phys']} {vals}"] += 1
        if m := RE_VFETCH.search(line):
            vfetches[f"idx={m['idx']} reg={m['reg']} phys={m['phys']} bytes={m['bytes']} endian={m['endian']}"] += 1
        if m := RE_TFETCH.search(line):
            tfetches[
                f"idx={m['idx']} base={m['base']} mip={m['mip']} {m['w']}x{m['h']} "
                f"fmt={m['fmt']} dim={m['dim']} pitch={m['pitch']} tiled={m['tiled']}"
            ] += 1
        if m := RE_DRAWCORR.search(line):
            corr_key = f"op={m['op']} cnt={m['cnt']}"
            drawcorr[f"{corr_key} vf={m['vf']} tf={m['tf']}"] += 1
        elif corr_key and (m := RE_CORR_V.search(line)):
            drawcorr_v[f"{corr_key} V{m['idx']} phys={m['phys']} bytes={m['bytes']}"] += 1
        elif corr_key and (m := RE_CORR_T.search(line)):
            drawcorr_t[
                f"{corr_key} T{m['idx']} base={m['base']} {m['w']}x{m['h']} "
                f"fmt={m['fmt']}"
            ] += 1
        elif "DRAWCORR" not in line and "      V" not in line and "      T" not in line:
            corr_key = ""
        if m := RE_LOADCTX.search(line):
            loadctx[f"{m['phys']} words={m['words']} offs={m['offs']}"] += 1
        if m := RE_STATE.search(line):
            states[f"base={m['base']} cnt={m['cnt']}"] += int(m["hits"])

    print(f"log: {path}")
    print(f"lines: {len(lines)}")
    print(f"live ring windows: {len(live)}")
    for num, lr, r6, rb, dw in live[: args.limit]:
        print(f"  LIVERB#{num} lr={lr} r6={r6} rb={rb} +{dw}dw")

    print(f"root/nested IB headers: {len(ibs)}")
    for num, phys, dw in ibs[: args.limit]:
        print(f"  IB#{num} phys={phys} dwords={dw}")

    sections = [
        ("rect strips", rects),
        ("strip setup", strips),
        ("tile keys", tiles),
        ("draw2", draw2),
        ("vertex fetches", vfetches),
        ("texture fetches", tfetches),
        ("draw correlations", drawcorr),
        ("draw corr vertex", drawcorr_v),
        ("draw corr texture", drawcorr_t),
        ("fetch samples", fetches),
        ("load ctx", loadctx),
        ("state bases", states),
        ("flags", flags),
    ]
    for title, counter in sections:
        print(f"{title}: {sum(counter.values())} total, {len(counter)} unique")
        for key, count in first_items(counter, args.limit):
            print(f"  {count:5d}  {key}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
