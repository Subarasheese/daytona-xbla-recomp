#!/usr/bin/env python3
"""Extract the local Daytona USA XBLA STFS package.

This is intentionally narrow: it extracts the known LIVE/STFS package layout
used by the XBLA release and writes the files needed by this repository.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path


BLOCK_SIZE = 0x1000
FILE_TABLE_OFFSET = 0xC000
FILE_TABLE_ENTRY_SIZE = 0x40
ROOT_PARENT = 0xFFFF
END_OF_CHAIN = 0xFFFFFF


class Entry:
    def __init__(
        self,
        index: int,
        name: str,
        flags: int,
        blocks: int,
        start_block: int,
        parent: int,
        size: int,
    ) -> None:
        self.index = index
        self.name = name
        self.flags = flags
        self.blocks = blocks
        self.start_block = start_block
        self.parent = parent
        self.size = size

    @property
    def is_directory(self) -> bool:
        return (self.flags & 0x80) != 0


def physical_block(logical_block: int) -> int:
    """Map an STFS logical data block to its physical package block."""
    group = logical_block // 0xAA
    level1_groups = group // 0xAA
    level1_overhead = level1_groups + 1 if level1_groups > 0 else 0
    return logical_block + 0x0C + group + (1 if group > 0 else 0) + level1_overhead


def physical_offset(logical_block: int) -> int:
    return physical_block(logical_block) * BLOCK_SIZE


def read_uint24_le(data: bytes) -> int:
    return data[0] | (data[1] << 8) | (data[2] << 16)


def read_uint24_be(data: bytes) -> int:
    return (data[0] << 16) | (data[1] << 8) | data[2]


def hash_entry_offset(logical_block: int) -> int:
    group = logical_block // 0xAA
    index = logical_block % 0xAA
    level1_groups = group // 0xAA
    level1_overhead = level1_groups + 1 if level1_groups > 0 else 0
    table_block = 0x0B + (group * 0xAB) + (1 if group > 0 else 0) + level1_overhead
    return (table_block * BLOCK_SIZE) + (index * 0x18)


def next_block(source, logical_block: int) -> int:
    source.seek(hash_entry_offset(logical_block) + 0x15)
    return read_uint24_be(source.read(3))


def parse_entries(package: Path) -> list[Entry]:
    entries: list[Entry] = []
    with package.open("rb") as f:
        f.seek(FILE_TABLE_OFFSET)
        index = 0
        while True:
            raw = f.read(FILE_TABLE_ENTRY_SIZE)
            if len(raw) != FILE_TABLE_ENTRY_SIZE or raw == b"\x00" * FILE_TABLE_ENTRY_SIZE:
                break

            name_flags = raw[0x28]
            name_len = name_flags & 0x3F
            if name_len == 0:
                break

            name = raw[:name_len].decode("ascii", errors="strict")
            entries.append(
                Entry(
                    index=index,
                    name=name,
                    flags=name_flags,
                    blocks=read_uint24_le(raw[0x29:0x2C]),
                    start_block=read_uint24_le(raw[0x2F:0x32]),
                    parent=int.from_bytes(raw[0x32:0x34], "big"),
                    size=int.from_bytes(raw[0x34:0x38], "big"),
                )
            )
            index += 1

    if not any(entry.name == "default.xex" for entry in entries):
        raise RuntimeError("could not find default.xex in the STFS file table")
    return entries


def entry_path(entry: Entry, entries: list[Entry]) -> Path:
    parts = [entry.name]
    parent = entry.parent
    seen = {entry.index}
    while parent != ROOT_PARENT:
        if parent >= len(entries) or parent in seen:
            raise RuntimeError(f"invalid parent chain for {entry.name}")
        parent_entry = entries[parent]
        parts.append(parent_entry.name)
        seen.add(parent)
        parent = parent_entry.parent
    return Path(*reversed(parts))


def extract_file(package: Path, entry: Entry, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    remaining = entry.size
    blocks_to_copy = max(entry.blocks, math.ceil(entry.size / BLOCK_SIZE))
    logical_block = entry.start_block

    with package.open("rb") as source, destination.open("wb") as output:
        for block_index in range(blocks_to_copy):
            if remaining <= 0:
                break
            if logical_block == END_OF_CHAIN:
                raise RuntimeError(f"unexpected end of block chain while extracting {entry.name}")

            source.seek(physical_offset(logical_block))
            chunk = source.read(min(BLOCK_SIZE, remaining))
            if not chunk:
                raise RuntimeError(f"unexpected EOF while extracting {entry.name}")
            output.write(chunk)
            remaining -= len(chunk)

            if block_index + 1 < blocks_to_copy:
                logical_block = next_block(source, logical_block)


def find_default_package(repo_root: Path) -> Path:
    game_dir = repo_root / "game"
    if not game_dir.is_dir():
        raise RuntimeError("no game/ directory found; place the XBLA package there first")
    candidates = sorted(path for path in game_dir.iterdir() if path.is_file())
    if not candidates:
        raise RuntimeError("no package file found in game/")
    if len(candidates) > 1:
        raise RuntimeError("multiple files found in game/; pass the package path explicitly")
    return candidates[0]


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "package",
        nargs="?",
        type=Path,
        help="Path to the local XBLA LIVE/STFS package. Defaults to the only file in game/.",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=repo_root / "extracted",
        help="Directory for extracted package contents.",
    )
    parser.add_argument(
        "--assets",
        type=Path,
        default=repo_root / "assets",
        help="Directory that receives default.xex for ReXGlue.",
    )
    args = parser.parse_args()

    package = (args.package or find_default_package(repo_root)).resolve()
    with package.open("rb") as f:
        if f.read(4) != b"LIVE":
            raise RuntimeError(f"{package} is not an Xbox 360 LIVE/STFS package")

    entries = parse_entries(package)
    extracted = 0
    default_xex: Path | None = None

    for entry in entries:
        relative = entry_path(entry, entries)
        destination = args.out / relative
        if entry.is_directory:
            destination.mkdir(parents=True, exist_ok=True)
            continue
        extract_file(package, entry, destination)
        extracted += 1
        if entry.name == "default.xex" and entry.parent == ROOT_PARENT:
            default_xex = destination

    if default_xex is None:
        raise RuntimeError("extracted package did not contain root default.xex")

    args.assets.mkdir(parents=True, exist_ok=True)
    asset_xex = args.assets / "default.xex"
    asset_xex.write_bytes(default_xex.read_bytes())

    print(f"Extracted {extracted} files to {args.out}")
    print(f"Wrote {asset_xex}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
