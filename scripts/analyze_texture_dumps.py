#!/usr/bin/env python3
"""Summarize raw Daytona texture dumps written by the renderer probes."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?", default="logs/texture_dumps/manifest.tsv")
    parser.add_argument("-o", "--output", default="logs/texture_dumps/summary.json")
    parser.add_argument("--no-hash", action="store_true")
    args = parser.parse_args()

    manifest = Path(args.manifest)
    rows: list[dict[str, object]] = []
    fieldnames = [
        "base", "mip", "width", "height", "format", "dimension",
        "pitch", "tiled", "bytes", "path",
    ]
    with manifest.open(newline="") as f:
        first = f.readline()
        f.seek(0)
        has_header = first.startswith("base\tmip\twidth\t")
        reader = csv.DictReader(
            f,
            delimiter="\t",
            fieldnames=None if has_header else fieldnames,
        )
        for row in reader:
            path = Path(row["path"])
            item: dict[str, object] = {
                "base": row["base"],
                "mip": row["mip"],
                "width": int(row["width"]),
                "height": int(row["height"]),
                "format": int(row["format"]),
                "dimension": int(row["dimension"]),
                "pitch": int(row["pitch"]),
                "tiled": bool(int(row["tiled"])),
                "bytes": int(row["bytes"]),
                "path": str(path),
                "exists": path.exists(),
            }
            if path.exists():
                item["file_bytes"] = path.stat().st_size
                if not args.no_hash:
                    item["sha256"] = sha256_file(path)
            rows.append(item)

    by_format = Counter(str(row["format"]) for row in rows)
    by_size = Counter(f"{row['width']}x{row['height']}" for row in rows)
    summary = {
        "manifest": str(manifest),
        "count": len(rows),
        "total_bytes": sum(int(row["bytes"]) for row in rows),
        "formats": dict(by_format),
        "sizes": dict(by_size.most_common()),
        "textures": rows,
    }

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

    print(f"wrote {output}")
    print(f"textures={summary['count']} total_bytes={summary['total_bytes']}")
    print("formats", dict(by_format))
    for row in sorted(rows, key=lambda r: int(r["bytes"]), reverse=True)[:12]:
        print(
            f"{row['base']} {row['width']}x{row['height']} fmt={row['format']} "
            f"bytes={row['bytes']} {row['path']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
