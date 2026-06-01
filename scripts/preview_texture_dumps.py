#!/usr/bin/env python3
"""Create inspectable previews from Daytona raw Xenos texture dumps."""

from __future__ import annotations

import argparse
import csv
import math
import struct
from pathlib import Path


FORMATS = {
    6: ("8_8_8_8", 1, 1, 4),
    10: ("8_8", 1, 1, 2),
    20: ("DXT5", 4, 4, 16),
}


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def log2_bytes_per_block(bytes_per_block: int) -> int:
    # Matches rex::texture_conversion::Untile.
    return (bytes_per_block // 4) + ((bytes_per_block // 2) >> (bytes_per_block // 4))


def tiled_offset_2d(x: int, y: int, pitch: int, log2_bpp: int) -> int:
    # Ported from rex::graphics::texture_util::GetTiledOffset2D.
    pitch = align(pitch, 32)
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (log2_bpp + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << log2_bpp
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return (
        ((offset & ~0x1FF) << 3)
        + ((y & 16) << 7)
        + ((offset & 0x1C0) << 2)
        + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)
        + (offset & 0x3F)
    )


def untile_2d(data: bytes, width_blocks: int, height_blocks: int, pitch_blocks: int,
              bytes_per_block: int) -> bytes:
    out = bytearray(width_blocks * height_blocks * bytes_per_block)
    log2_bpp = log2_bytes_per_block(bytes_per_block)
    for y in range(height_blocks):
        row_out = y * width_blocks * bytes_per_block
        for x in range(width_blocks):
            src = tiled_offset_2d(x, y, pitch_blocks, log2_bpp)
            dst = row_out + x * bytes_per_block
            src_end = src + bytes_per_block
            if src_end <= len(data):
                out[dst:dst + bytes_per_block] = data[src:src_end]
    return bytes(out)


def parse_manifest(path: Path) -> list[dict[str, object]]:
    fieldnames = [
        "base", "mip", "width", "height", "format", "dimension",
        "pitch", "tiled", "bytes", "path",
    ]
    rows: list[dict[str, object]] = []
    with path.open(newline="") as f:
        first = f.readline()
        f.seek(0)
        has_header = first.startswith("base\tmip\twidth\t")
        reader = csv.DictReader(
            f,
            delimiter="\t",
            fieldnames=None if has_header else fieldnames,
        )
        for row in reader:
            rows.append({
                "base": row["base"],
                "width": int(row["width"]),
                "height": int(row["height"]),
                "format": int(row["format"]),
                "pitch": int(row["pitch"]),
                "tiled": bool(int(row["tiled"])),
                "path": Path(row["path"]),
            })
    return rows


def dds_header_dxt5(width: int, height: int, linear_size: int) -> bytes:
    # DDS_HEADER with DDS_PIXELFORMAT FOURCC DXT5.
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000
    caps = 0x1000
    ddspf = struct.pack("<II4sIIIII", 32, 0x4, b"DXT5", 0, 0, 0, 0, 0)
    header = struct.pack(
        "<I"      # size
        "I"       # flags
        "I"       # height
        "I"       # width
        "I"       # pitch_or_linear_size
        "I"       # depth
        "I"       # mipmap_count
        "11I"     # reserved1
        "32s"     # ddspf
        "I"       # caps
        "I"       # caps2
        "I"       # caps3
        "I"       # caps4
        "I",      # reserved2
        124, flags, height, width, linear_size, 0, 0,
        *([0] * 11),
        ddspf,
        caps, 0, 0, 0, 0,
    )
    return b"DDS " + header


def write_fmt20_dds(row: dict[str, object], linear: bytes, output: Path) -> None:
    width = int(row["width"])
    height = int(row["height"])
    block_w = math.ceil(width / 4)
    block_h = math.ceil(height / 4)
    linear_size = block_w * block_h * 16
    output.write_bytes(dds_header_dxt5(width, height, linear_size) + linear[:linear_size])


def write_fmt6_ppm(row: dict[str, object], linear: bytes, output: Path) -> None:
    width = int(row["width"])
    height = int(row["height"])
    rgb = bytearray(width * height * 3)
    for i in range(width * height):
        p = i * 4
        q = i * 3
        # Xenos k_8_8_8_8 commonly lands in BGRA-like byte order after the dump.
        b, g, r = linear[p], linear[p + 1], linear[p + 2]
        rgb[q:q + 3] = bytes((r, g, b))
    output.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + rgb)


def write_fmt10_pgm(row: dict[str, object], linear: bytes, output: Path) -> None:
    width = int(row["width"])
    height = int(row["height"])
    gray = bytearray(width * height)
    for i in range(width * height):
        gray[i] = linear[i * 2]
    output.write_bytes(f"P5\n{width} {height}\n255\n".encode("ascii") + gray)


def preview_one(row: dict[str, object], output_dir: Path) -> Path | None:
    fmt = int(row["format"])
    if fmt not in FORMATS:
        return None
    src = Path(row["path"])
    if not src.exists():
        return None

    _name, block_w, block_h, bpb = FORMATS[fmt]
    width = int(row["width"])
    height = int(row["height"])
    width_blocks = math.ceil(width / block_w)
    height_blocks = math.ceil(height / block_h)
    pitch_texels = int(row["pitch"]) * 32
    pitch_blocks = max(width_blocks, math.ceil(pitch_texels / block_w))
    data = src.read_bytes()
    linear = untile_2d(data, width_blocks, height_blocks, pitch_blocks, bpb) if row["tiled"] else data

    stem = f"tex_{row['base']}_{width}x{height}_fmt{fmt}"
    if fmt == 20:
        out = output_dir / f"{stem}.dds"
        write_fmt20_dds(row, linear, out)
    elif fmt == 6:
        out = output_dir / f"{stem}.ppm"
        write_fmt6_ppm(row, linear, out)
    elif fmt == 10:
        out = output_dir / f"{stem}.pgm"
        write_fmt10_pgm(row, linear, out)
    else:
        return None
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?", default="logs/texture_dumps/manifest.tsv")
    parser.add_argument("-o", "--output-dir", default="logs/texture_dumps/previews")
    parser.add_argument("--limit", type=int, default=0, help="maximum textures to convert; 0 means all")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = parse_manifest(Path(args.manifest))
    written: list[Path] = []
    skipped = 0
    for row in rows:
        if args.limit and len(written) >= args.limit:
            break
        out = preview_one(row, output_dir)
        if out:
            written.append(out)
        else:
            skipped += 1

    print(f"wrote {len(written)} previews to {output_dir}")
    if skipped:
        print(f"skipped {skipped} rows")
    for path in written[:20]:
        print(path)
    if len(written) > 20:
        print(f"... {len(written) - 20} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
