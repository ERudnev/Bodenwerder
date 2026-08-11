#!/usr/bin/env python3
"""LWO3 meshpack helper (LightWave 2018+/2020).

Agent workflow after LW edits: run this, then rewrite *.lwo.meshpack parts from surfaces.
Assimp skips SURF.BLOK Image Maps — this script reads CLIP/STIL + IMAP/IMAG and LAYR names/pivots.

Usage:
  python lwo_surf_textures.py <path.lwo>
  python lwo_surf_textures.py <path.lwo> --meshpack-snippet
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path


def _u2(data: bytes, off: int) -> int:
    return int.from_bytes(data[off : off + 2], "big")


def _u4(data: bytes, off: int) -> int:
    return int.from_bytes(data[off : off + 4], "big")


def _f4(data: bytes, off: int) -> float:
    return struct.unpack(">f", data[off : off + 4])[0]


def _padded_c_string(data: bytes, off: int, limit: int) -> tuple[str, int]:
    end = data.find(b"\x00", off, off + limit)
    if end < 0:
        end = off + limit
    text = data[off:end].decode("ascii", "replace")
    # LWO strings are even-padded including the null.
    consumed = end - off + 1
    if consumed % 2:
        consumed += 1
    return text, off + consumed


def _basename(path_text: str) -> str:
    normalized = path_text.replace("\\", "/").rstrip("\x00")
    return Path(normalized).name


def parse_clips(data: bytes) -> dict[int, str]:
    clips: dict[int, str] = {}
    pos = 0
    while True:
        j = data.find(b"FORM", pos)
        if j < 0:
            break
        form_sz = _u4(data, j + 4)
        if data[j + 8 : j + 12] == b"CLIP":
            body = data[j + 12 : j + 8 + form_sz]
            # LWO3: subchunk '    ' size 4 → clip index, then nested FORM STIL …
            idx = None
            stil_path = None
            off = 0
            while off + 8 <= len(body):
                tag = body[off : off + 4]
                if tag == b"FORM":
                    nested_sz = _u4(body, off + 4)
                    nested_type = body[off + 8 : off + 12]
                    nested = body[off + 12 : off + 8 + nested_sz]
                    if nested_type == b"STIL" and nested[0:4] == b"    ":
                        name_sz = _u4(nested, 4)
                        stil_path = nested[8 : 8 + name_sz].split(b"\x00")[0].decode("ascii", "replace")
                    off += 8 + nested_sz
                    continue
                size = _u4(body, off + 4)
                payload = body[off + 8 : off + 8 + size]
                if tag == b"    " and size >= 4:
                    idx = _u4(payload, 0)
                off += 8 + size
                if size % 2:
                    off += 1
            if idx is not None and stil_path:
                clips[idx] = stil_path
        pos = j + 4
    return clips


def parse_surfaces(data: bytes, clips: dict[int, str]) -> list[dict]:
    surfaces: list[dict] = []
    pos = 0
    while True:
        j = data.find(b"FORM", pos)
        if j < 0:
            break
        form_sz = _u4(data, j + 4)
        if data[j + 8 : j + 12] == b"SURF" and data[j + 12 : j + 16] == b"    ":
            name_sz = _u4(data, j + 16)
            name = data[j + 20 : j + 20 + name_sz].split(b"\x00")[0].decode("ascii", "replace")
            body = data[j + 12 : j + 8 + form_sz]
            textures: list[str] = []
            # Prefer classic Texture Editor Image Maps (BLOK/IMAP/IMAG).
            search_from = 0
            while True:
                imag = body.find(b"IMAG", search_from)
                if imag < 0:
                    break
                # LWO3 nested: IMAG + U4 size + U2 clip index (size usually 2).
                if imag + 10 <= len(body):
                    size = _u4(body, imag + 4)
                    if size >= 2:
                        clip = _u2(body, imag + 8)
                        path = clips.get(clip)
                        if path:
                            file = _basename(path)
                            if file not in textures:
                                textures.append(file)
                search_from = imag + 4
            surfaces.append({"name": name, "textures": textures})
        pos = j + 4
    return surfaces


def parse_layers(data: bytes) -> list[dict]:
    """Parse LAYR chunks: number, flags, pivot (VEC12), name. Unique by name, first wins."""
    layers: list[dict] = []
    seen: set[str] = set()
    pos = 0
    while True:
        j = data.find(b"LAYR", pos)
        if j < 0:
            break
        if j + 8 > len(data):
            break
        size = _u4(data, j + 4)
        body = data[j + 8 : j + 8 + size]
        if len(body) < 16:
            pos = j + 4
            continue
        number = _u2(body, 0)
        flags = _u2(body, 2)
        pivot = (_f4(body, 4), _f4(body, 8), _f4(body, 12))
        name, _ = _padded_c_string(body, 16, len(body) - 16)
        if name and name not in seen:
            seen.add(name)
            layers.append({"name": name, "number": number, "flags": flags, "pivot": pivot})
        pos = j + 4
    return layers


def meshpack_parts(surfaces: list[dict], material: str = "rmmr::lit_textured") -> list[list]:
    parts: list[list] = []
    for surface in surfaces:
        textures = surface["textures"]
        albedo = textures[0] if textures else "MISSING_TEXTURE"
        parts.append([surface["name"], [material, [["albedoMap", albedo]]]])
    return parts


def main() -> int:
    parser = argparse.ArgumentParser(description="Inspect LWO3 surfaces, Color Image Map textures, and LAYR pivots.")
    parser.add_argument("lwo", type=Path, help="path to .lwo")
    parser.add_argument("--meshpack-snippet", action="store_true", help="print only the meshpack parts array JSON")
    parser.add_argument("--material", default="rmmr::lit_textured", help="engine material Unit::Name for snippet")
    args = parser.parse_args()

    path: Path = args.lwo
    if not path.is_file():
        print(f"error: file not found: {path}", file=sys.stderr)
        return 1
    data = path.read_bytes()
    if data[8:12] not in (b"LWO2", b"LWO3", b"LWOB"):
        print(f"error: not an LWO object (got {data[8:12]!r})", file=sys.stderr)
        return 1

    clips = parse_clips(data)
    surfaces = parse_surfaces(data, clips)
    layers = parse_layers(data)

    if args.meshpack_snippet:
        print(json.dumps(meshpack_parts(surfaces, args.material), indent=4, ensure_ascii=False))
        return 0

    print(f"file: {path}")
    print(f"format: {data[8:12].decode()}")
    print(f"clips ({len(clips)}):")
    for idx in sorted(clips):
        print(f"  [{idx}] {_basename(clips[idx])}  ({clips[idx]})")
    print(f"surfaces ({len(surfaces)}):")
    for surface in surfaces:
        if surface["textures"]:
            print(f"  {surface['name']}: {', '.join(surface['textures'])}")
        else:
            print(f"  {surface['name']}: (no Image Map textures)")
    print(f"layers / meshes ({len(layers)}):")
    for layer in layers:
        px, py, pz = layer["pivot"]
        print(f"  {layer['name']}: pivot=({px:g}, {py:g}, {pz:g})  layr={layer['number']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
