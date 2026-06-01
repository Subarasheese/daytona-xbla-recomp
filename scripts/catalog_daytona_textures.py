#!/usr/bin/env python3
"""Merge Daytona TFETCH draw usage with dumped texture preview paths."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


FORMAT_NAMES = {
    6: "8_8_8_8",
    10: "8_8",
    20: "DXT5",
}


PREVIEW_EXT = {
    6: ".ppm",
    10: ".pgm",
    20: ".dds",
}


def parse_manifest(path: Path) -> dict[tuple[object, ...], dict[str, object]]:
    fieldnames = [
        "base", "mip", "width", "height", "format", "dimension",
        "pitch", "tiled", "bytes", "path",
    ]
    textures: dict[tuple[object, ...], dict[str, object]] = {}
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
            key = (
                row["base"],
                row["mip"],
                int(row["width"]),
                int(row["height"]),
                int(row["format"]),
                int(row["dimension"]),
                int(row["pitch"]),
                bool(int(row["tiled"])),
            )
            textures[key] = {
                "base": row["base"],
                "mip": row["mip"],
                "width": int(row["width"]),
                "height": int(row["height"]),
                "format": int(row["format"]),
                "dimension": int(row["dimension"]),
                "pitch": int(row["pitch"]),
                "tiled": bool(int(row["tiled"])),
                "dump_bytes": int(row["bytes"]),
                "raw_path": row["path"],
            }
    return textures


def preview_path(row: dict[str, object], preview_dir: Path) -> str:
    ext = PREVIEW_EXT.get(int(row["format"]))
    if not ext:
        return ""
    return str(
        preview_dir /
        f"tex_{row['base']}_{row['width']}x{row['height']}_fmt{row['format']}{ext}"
    )


def texture_key(row: dict[str, object]) -> tuple[object, ...]:
    return (
        row["base"],
        row["mip"],
        int(row["width"]),
        int(row["height"]),
        int(row["format"]),
        int(row["dimension"]),
        int(row["pitch"]),
        bool(row["tiled"]),
    )


def write_markdown(catalog: list[dict[str, object]], path: Path) -> None:
    lines = [
        "# Daytona Texture Catalog",
        "",
        "Sorted by draw usage, then dumped-only textures.",
        "",
        "| Uses | Base | Size | Format | Slots | Ops | Preview | Raw |",
        "|---:|---|---:|---|---|---|---|---|",
    ]
    for row in catalog:
        uses = int(row.get("draw_uses", 0))
        slots = ", ".join(f"T{k}:{v}" for k, v in sorted(row.get("slots", {}).items()))
        ops = ", ".join(f"{k}:{v}" for k, v in sorted(row.get("ops", {}).items()))
        preview = row.get("preview_path", "")
        raw = row.get("raw_path", "")
        preview_link = f"[preview]({preview})" if preview else ""
        raw_link = f"[raw]({raw})" if raw else ""
        lines.append(
            f"| {uses} | `{row['base']}` | {row['width']}x{row['height']} | "
            f"{FORMAT_NAMES.get(int(row['format']), row['format'])} | {slots} | {ops} | "
            f"{preview_link} | {raw_link} |"
        )
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frame", default="logs/daytona_frame_model.json")
    parser.add_argument("--manifest", default="logs/texture_dumps/manifest.tsv")
    parser.add_argument("--preview-dir", default="logs/texture_dumps/previews")
    parser.add_argument("-o", "--output", default="logs/daytona_texture_catalog.json")
    parser.add_argument("--markdown", default="logs/daytona_texture_catalog.md")
    args = parser.parse_args()

    frame = json.loads(Path(args.frame).read_text())
    manifest = parse_manifest(Path(args.manifest))
    preview_dir = Path(args.preview_dir)

    merged: dict[tuple[object, ...], dict[str, object]] = {}
    for key, row in manifest.items():
        item = dict(row)
        item.update({
            "format_name": FORMAT_NAMES.get(int(row["format"]), f"fmt{row['format']}"),
            "preview_path": preview_path(row, preview_dir),
            "draw_uses": 0,
            "slots": {},
            "ops": {},
            "source": "dump",
        })
        merged[key] = item

    for tex in frame.get("textures", []):
        key = texture_key(tex)
        item = merged.setdefault(key, {
            "base": tex["base"],
            "mip": tex["mip"],
            "width": tex["width"],
            "height": tex["height"],
            "format": tex["format"],
            "dimension": tex["dimension"],
            "pitch": tex["pitch"],
            "tiled": tex["tiled"],
            "dump_bytes": 0,
            "raw_path": "",
            "format_name": FORMAT_NAMES.get(int(tex["format"]), f"fmt{tex['format']}"),
            "preview_path": preview_path(tex, preview_dir),
            "source": "draw",
        })
        item["draw_uses"] = int(tex.get("draw_uses", 0))
        item["slots"] = tex.get("slots", {})
        item["ops"] = tex.get("ops", {})
        if item.get("source") == "dump":
            item["source"] = "dump+draw"

    catalog = sorted(
        merged.values(),
        key=lambda row: (-int(row.get("draw_uses", 0)), str(row["base"]), int(row["format"])),
    )

    output = Path(args.output)
    output.write_text(json.dumps({
        "source_frame": args.frame,
        "source_manifest": args.manifest,
        "count": len(catalog),
        "with_draws": sum(1 for row in catalog if int(row.get("draw_uses", 0)) > 0),
        "catalog": catalog,
    }, indent=2, sort_keys=True) + "\n")

    markdown = Path(args.markdown)
    write_markdown(catalog, markdown)

    print(f"wrote {output}")
    print(f"wrote {markdown}")
    print(f"textures={len(catalog)} with_draws={sum(1 for row in catalog if int(row.get('draw_uses', 0)) > 0)}")
    for row in catalog[:12]:
        print(
            f"{row.get('draw_uses', 0):4} {row['base']} "
            f"{row['width']}x{row['height']} {row['format_name']} "
            f"{row.get('preview_path', '')}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
