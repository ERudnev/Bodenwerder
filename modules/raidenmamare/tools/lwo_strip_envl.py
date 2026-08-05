#!/usr/bin/env python3
"""Strip LWO/LWO3 envelope data (ENVL / ENVS / ENVD) for static mesh import.

Assimp's LWO3 envelope parser rejects some LightWave node-surface Color.*
envelopes. We do not use LWO animation — remove those chunks from the file.

Usage:
  python lwo_strip_envl.py path/to/file.lwo
  python lwo_strip_envl.py path/to/file.lwo --dry-run
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# Bare chunk tags and FORM subtypes to drop (envelope system).
DROP_CHUNK = {b"ENVL"}
DROP_FORM = {b"ENVL", b"ENVS", b"ENVD"}


def rebuild_chunk_list(data: bytes, start: int, end: int, stats: dict) -> bytes:
    out = bytearray()
    i = start
    while i + 8 <= end:
        tag = data[i : i + 4]
        size = struct.unpack(">I", data[i + 4 : i + 8])[0]
        body = i + 8
        payload_end = body + size
        if payload_end > end:
            raise ValueError(f"chunk {tag!r} at {i} length {size} overruns parent end {end}")
        nxt = payload_end + (size & 1)

        if tag in DROP_CHUNK:
            stats["dropped"] += 1
            i = nxt
            continue

        if tag == b"FORM":
            if size < 4:
                raise ValueError(f"FORM at {i} too small")
            form_type = data[body : body + 4]
            if form_type in DROP_FORM:
                stats["dropped"] += 1
                i = nxt
                continue
            inner = rebuild_chunk_list(data, body + 4, payload_end, stats)
            new_size = 4 + len(inner)
            out += b"FORM"
            out += struct.pack(">I", new_size)
            out += form_type
            out += inner
            if new_size & 1:
                out += b"\0"
            i = nxt
            continue

        out += data[i:payload_end]
        if size & 1:
            out += b"\0"
        i = nxt

    return bytes(out)


def strip_envl(data: bytes) -> tuple[bytes, dict]:
    if len(data) < 12 or data[0:4] != b"FORM":
        raise ValueError("not an IFF FORM file")
    root_size = struct.unpack(">I", data[4:8])[0]
    root_type = data[8:12]
    if root_type not in (b"LWO2", b"LWO3", b"LWOB"):
        raise ValueError(f"unexpected root type {root_type!r}")
    end = 8 + root_size
    if end > len(data):
        raise ValueError("FORM size larger than file")

    stats = {"dropped": 0}
    inner = rebuild_chunk_list(data, 12, end, stats)
    new_size = 4 + len(inner)
    out = b"FORM" + struct.pack(">I", new_size) + root_type + inner
    stats["before"] = len(data)
    stats["after"] = len(out)
    return out, stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="path to .lwo")
    parser.add_argument("--dry-run", action="store_true", help="report only, do not write")
    args = parser.parse_args()

    path: Path = args.path
    data = path.read_bytes()
    try:
        out, stats = strip_envl(data)
    except ValueError as error:
        print(f"error: {path}: {error}", file=sys.stderr)
        return 1

    print(f"{path}: dropped {stats['dropped']} envelope chunk(s)/FORM(s); {stats['before']} → {stats['after']} bytes")
    if stats["dropped"] == 0:
        print("nothing to strip")
        return 0
    if args.dry_run:
        print("dry-run: not written")
        return 0

    path.write_bytes(out)
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
