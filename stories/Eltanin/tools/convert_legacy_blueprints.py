#!/usr/bin/env python3
"""One-shot legacy blueprint → current Eltanin blueprint format.

Legacy (classic):
  { name, author, [volumetrics], [wings], [plates] }
  volumetric/plate: [[x,y,z], ori, shape, slot|role]

Current:
  { name, author, [cells], [mounts] }
  cell:   [[x,y,z], ori, shape, corners[], halfribs[], membranes[]]
  mount:  [\"Eltanin::mounts.<stem>\", [gx,gy,gz], rotation]

Policy:
  - k8/k7/k6/k4 volumetrics → seeded skeleton cell (recipes ≡ seedCorners/seedHalfribs)
  - k8 with Role slot → also dummy_<Role> mount (internal module stand-in)
  - non-k8 → skeleton only (legacy internal modules dropped, frame kept)
  - hangar → skeleton only, no dummy (hand-fill later)
  - wings → flat skeleton cells (w1111→k4f1111, w121→k3f121, w2121→k4f2121, w321→k3f222); no dummy
  - plates → armor mounts (p*_default); kept even if volumetric cell was dropped
  - membranes always empty

Mount transform:
  Legacy stored Placement {cell, ori} for ±2 m home-cube seating (actorPose / corner 0).
  Current mounts use Transform {grid, rotation} with gridActorPose (pivot at grid*edge).
  Conversion: grid = cell + corners[cornerIndex(ori, 0)], rotation = ori.
  Copying cell as grid spins the plate around the wrong corner.
"""

from __future__ import annotations

import argparse
import ast
from pathlib import Path

# seedCorners / seedHalfribs mirrors of subframe.h recipes (pole pair per edge).
SEEDS: dict[str, tuple[list[tuple[str, int]], list[tuple[str, str, int]]]] = {
    "k8": (
        [
            ("c124", 0),
            ("c124", 4),
            ("c124", 3),
            ("c124", 7),
            ("c124", 1),
            ("c124", 5),
            ("c124", 2),
            ("c124", 6),
        ],
        [
            ("he1deg90", "starts", 0),
            ("he1deg90", "ends", 0),
            ("he1deg90", "starts", 15),
            ("he1deg90", "ends", 15),
            ("he1deg90", "starts", 16),
            ("he1deg90", "ends", 16),
            ("he1deg90", "starts", 21),
            ("he1deg90", "ends", 21),
            ("he1deg90", "starts", 4),
            ("he1deg90", "ends", 4),
            ("he1deg90", "starts", 3),
            ("he1deg90", "ends", 3),
            ("he1deg90", "starts", 14),
            ("he1deg90", "ends", 14),
            ("he1deg90", "starts", 20),
            ("he1deg90", "ends", 20),
            ("he1deg90", "starts", 1),
            ("he1deg90", "ends", 1),
            ("he1deg90", "starts", 17),
            ("he1deg90", "ends", 17),
            ("he1deg90", "starts", 5),
            ("he1deg90", "ends", 5),
            ("he1deg90", "starts", 2),
            ("he1deg90", "ends", 2),
        ],
    ),
    "k7": (
        [
            ("c1364", 0),
            ("c124", 4),
            ("c1364", 20),
            ("c124", 1),
            ("c124", 5),
            ("c1364", 13),
            ("c124", 6),
        ],
        [
            ("he1deg90", "starts", 0),
            ("he1deg90", "ends", 0),
            ("he3deg125", "starts", 0),
            ("he3deg125", "ends", 0),
            ("he1deg90", "starts", 16),
            ("he1deg90", "ends", 16),
            ("he3deg125", "ends", 13),
            ("he3deg125", "starts", 13),
            ("he1deg90", "starts", 21),
            ("he1deg90", "ends", 21),
            ("he1deg90", "starts", 4),
            ("he1deg90", "ends", 4),
            ("he3deg125", "starts", 20),
            ("he3deg125", "ends", 20),
            ("he1deg90", "starts", 20),
            ("he1deg90", "ends", 20),
            ("he1deg90", "starts", 1),
            ("he1deg90", "ends", 1),
            ("he1deg90", "starts", 17),
            ("he1deg90", "ends", 17),
            ("he1deg90", "starts", 5),
            ("he1deg90", "ends", 5),
            ("he1deg90", "starts", 2),
            ("he1deg90", "ends", 2),
        ],
    ),
    "k6": (
        [
            ("c164", 0),
            ("c134", 4),
            ("c124", 1),
            ("c124", 5),
            ("c134", 13),
            ("c164", 9),
        ],
        [
            ("he1deg45", "starts", 0),
            ("he1deg45", "ends", 0),
            ("he1deg90", "starts", 16),
            ("he1deg90", "ends", 16),
            ("he3deg90", "starts", 13),
            ("he3deg90", "ends", 13),
            ("he1deg90", "starts", 4),
            ("he1deg90", "ends", 4),
            ("he3deg90", "starts", 4),
            ("he3deg90", "ends", 4),
            ("he1deg90", "starts", 1),
            ("he1deg90", "ends", 1),
            ("he1deg90", "starts", 17),
            ("he1deg90", "ends", 17),
            ("he1deg90", "starts", 5),
            ("he1deg90", "ends", 5),
            ("he1deg45", "ends", 9),
            ("he1deg45", "starts", 9),
        ],
    ),
    "k4": (
        [
            ("c135", 4),
            ("c135", 1),
            ("c124", 5),
            ("c135", 23),
        ],
        [
            ("he3deg71", "ends", 1),
            ("he3deg71", "starts", 1),
            ("he1deg90", "starts", 4),
            ("he1deg90", "ends", 4),
            ("he3deg71", "starts", 4),
            ("he3deg71", "ends", 4),
            ("he1deg90", "starts", 1),
            ("he1deg90", "ends", 1),
            ("he3deg71", "ends", 23),
            ("he3deg71", "starts", 23),
            ("he1deg90", "starts", 5),
            ("he1deg90", "ends", 5),
        ],
    ),
    # Flat ex-wings (Space menu flats).
    "k4f1111": (
        [
            ("c12", 0),
            ("c12", 21),
            ("c12", 19),
            ("c12", 10),
        ],
        [
            ("he1deg90", "starts", 0),
            ("he1deg90", "ends", 0),
            ("he1deg90", "starts", 15),
            ("he1deg90", "ends", 15),
            ("he1deg90", "starts", 21),
            ("he1deg90", "ends", 21),
            ("he1deg90", "starts", 3),
            ("he1deg90", "ends", 3),
        ],
    ),
    "k3f121": (
        [
            ("c12", 0),
            ("c15", 11),
            ("c13", 19),
        ],
        [
            ("he1deg90", "starts", 0),
            ("he1deg90", "ends", 0),
            ("he1deg90", "starts", 15),
            ("he1deg90", "ends", 15),
            ("he3deg90", "ends", 19),
            ("he3deg90", "starts", 19),
        ],
    ),
    "k4f2121": (
        [
            ("c164", 0),
            ("c134", 4),
            ("c134", 13),
            ("c164", 9),
        ],
        [
            ("he1deg45", "starts", 0),
            ("he1deg45", "ends", 0),
            ("he3deg90", "ends", 13),
            ("he3deg90", "starts", 13),
            ("he3deg90", "starts", 4),
            ("he3deg90", "ends", 4),
            ("he1deg45", "ends", 9),
            ("he1deg45", "starts", 9),
        ],
    ),
    "k3f222": (
        [
            ("c135", 4),
            ("c135", 1),
            ("c135", 23),
        ],
        [
            ("he3deg71", "ends", 1),
            ("he3deg71", "starts", 1),
            ("he3deg71", "starts", 4),
            ("he3deg71", "ends", 4),
            ("he3deg71", "ends", 23),
            ("he3deg71", "starts", 23),
        ],
    ),
}

# Legacy wing shape → current flat frame::shape.
WING_TO_SHAPE = {
    "w1111": "k4f1111",
    "w121": "k3f121",
    "w2121": "k4f2121",
    "w321": "k3f222",  # legacy tag; recipe labeled ex-w222
}

# Legacy volumetric slot → Role / dummy stem. hangar = skeleton only.
SLOT_TO_ROLE = {
    "multi": "custom",
    "engine": "propulsion",
    "power": "power",
    "battery": "gyros",
    "hardpoint": "weaponry",
    "cargo": "cargo",
    "control": "control",
    "living": "living",
}

PLATE_TO_MOUNT = {
    "p1111": "p1111_default",
    "p121": "p121_default",
    "p2121": "p2121_default",
    "p222A": "p222A_default",
    "p222V": "p222V_default",
}

# cube::corners — {0,1}³ lattice (shapes.h).
CUBE_CORNERS = (
    (0, 0, 0),
    (1, 0, 0),
    (0, 1, 0),
    (1, 1, 0),
    (0, 0, 1),
    (1, 0, 1),
    (0, 1, 1),
    (1, 1, 1),
)

# space::orient::matrix rows (space.cpp) — row-major 3×3 in {-1,0,1}.
ORIENT_MATRIX = (
    ((1, 0, 0), (0, 1, 0), (0, 0, 1)),
    ((1, 0, 0), (0, 0, 1), (0, -1, 0)),
    ((1, 0, 0), (0, -1, 0), (0, 0, -1)),
    ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
    ((0, 0, -1), (0, 1, 0), (1, 0, 0)),
    ((0, 0, -1), (1, 0, 0), (0, -1, 0)),
    ((0, 0, -1), (0, -1, 0), (-1, 0, 0)),
    ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ((-1, 0, 0), (0, 1, 0), (0, 0, -1)),
    ((-1, 0, 0), (0, 0, -1), (0, -1, 0)),
    ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
    ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
    ((0, 0, 1), (0, 1, 0), (-1, 0, 0)),
    ((0, 0, 1), (-1, 0, 0), (0, -1, 0)),
    ((0, 0, 1), (0, -1, 0), (1, 0, 0)),
    ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
    ((0, 1, 0), (0, 0, 1), (1, 0, 0)),
    ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
    ((0, 1, 0), (0, 0, -1), (-1, 0, 0)),
    ((0, 1, 0), (-1, 0, 0), (0, 0, 1)),
    ((0, -1, 0), (0, 0, -1), (1, 0, 0)),
    ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
    ((0, -1, 0), (0, 0, 1), (-1, 0, 0)),
    ((0, -1, 0), (-1, 0, 0), (0, 0, -1)),
)


def mul_imat3(matrix, vec):
    r0, r1, r2 = matrix
    return (
        r0[0] * vec[0] + r0[1] * vec[1] + r0[2] * vec[2],
        r1[0] * vec[0] + r1[1] * vec[1] + r1[2] * vec[2],
        r2[0] * vec[0] + r2[1] * vec[1] + r2[2] * vec[2],
    )


def corner_index(ori: int, corner: int = 0) -> int:
    """space::orient::cornerIndex — where home cube corner lands after ori."""
    sx, sy, sz = CUBE_CORNERS[corner]
    signed = (sx * 2 - 1, sy * 2 - 1, sz * 2 - 1)
    wx, wy, wz = mul_imat3(ORIENT_MATRIX[ori], signed)
    want = ((wx + 1) // 2, (wy + 1) // 2, (wz + 1) // 2)
    for i, c in enumerate(CUBE_CORNERS):
        if c == want:
            return i
    return corner


def placement_to_transform(cell, ori: int) -> tuple[tuple[int, int, int], int]:
    """Legacy Placement {cell, ori} → Mount Transform {grid, rotation}."""
    seat = CUBE_CORNERS[corner_index(ori, 0)]
    grid = (cell[0] + seat[0], cell[1] + seat[1], cell[2] + seat[2])
    return grid, ori


def parse_legacy(text: str):
    text = text.strip()
    if not (text.startswith("{") and text.endswith("}")):
        raise ValueError("legacy blueprint must be a classic { … } document")
    data = ast.literal_eval("[" + text[1:-1] + "]")
    if len(data) != 5:
        raise ValueError(f"expected 5 top-level fields, got {len(data)}")
    name, author, volumetrics, wings, plates = data
    return name, author, volumetrics, wings, plates


def fmt_index3(cell) -> str:
    x, y, z = cell
    return f"[{x}, {y}, {z}]"


def fmt_corner(kind: str, ori: int) -> str:
    return f'["{kind}", {ori}]'


def fmt_half(kind: str, pole: str, ori: int) -> str:
    return f'["{kind}", "{pole}", {ori}]'


def fmt_list(items: list[str], indent: str) -> str:
    if not items:
        return "[]"
    inner = ",\n".join(f"{indent}    {item}" for item in items)
    return f"[\n{inner}\n{indent}]"


def format_cell(cell, ori: int, shape: str) -> str:
    corners_src, halfs_src = SEEDS[shape]
    corners = [fmt_corner(k, o) for k, o in corners_src]
    halfs = [fmt_half(k, p, o) for k, p, o in halfs_src]
    return (
        "        [\n"
        f"            {fmt_index3(cell)},\n"
        f"            {ori},\n"
        f'            "{shape}",\n'
        f"            {fmt_list(corners, '            ')},\n"
        f"            {fmt_list(halfs, '            ')},\n"
        f"            []\n"
        f"        ]"
    )


def convert(name: str, author: str, volumetrics, wings, plates):
    cells: list[str] = []
    mounts: list[str] = []
    stats = {
        "cells": 0,
        "k8": 0,
        "k7": 0,
        "k6": 0,
        "k4": 0,
        "k4f1111": 0,
        "k3f121": 0,
        "k4f2121": 0,
        "k3f222": 0,
        "wings": 0,
        "dropped_shape": 0,
        "dropped_wing": 0,
        "hangar_skel": 0,
        "dummies": 0,
        "plates": 0,
        "unknown_slot": 0,
        "unknown_plate": 0,
    }

    for cell, ori, shape, slot in volumetrics:
        if shape not in SEEDS:
            stats["dropped_shape"] += 1
            continue

        cells.append(format_cell(cell, ori, shape))
        stats["cells"] += 1
        stats[shape] += 1

        if slot == "hangar":
            stats["hangar_skel"] += 1
            continue

        # Internal-module stand-in only for full k8 cubes with a known Role.
        if shape != "k8":
            continue
        role = SLOT_TO_ROLE.get(slot)
        if role is None:
            stats["unknown_slot"] += 1
            continue

        grid, rotation = placement_to_transform(cell, ori)
        mounts.append(f'        ["Eltanin::mounts.dummy_{role}", {fmt_index3(grid)}, {rotation}]')
        stats["dummies"] += 1

    for cell, ori, wing, _role in wings:
        shape = WING_TO_SHAPE.get(wing)
        if shape is None or shape not in SEEDS:
            stats["dropped_wing"] += 1
            continue
        cells.append(format_cell(cell, ori, shape))
        stats["cells"] += 1
        stats[shape] += 1
        stats["wings"] += 1

    for cell, ori, plate, _armor in plates:
        stem = PLATE_TO_MOUNT.get(plate)
        if stem is None:
            stats["unknown_plate"] += 1
            continue
        grid, rotation = placement_to_transform(cell, ori)
        mounts.append(f'        ["Eltanin::mounts.{stem}", {fmt_index3(grid)}, {rotation}]')
        stats["plates"] += 1

    cells_body = "    [],\n" if not cells else "    [\n" + ",\n".join(cells) + "\n    ],\n"
    mounts_body = "    []\n" if not mounts else "    [\n" + ",\n".join(mounts) + "\n    ]\n"
    document = "{\n" f'    "{name}",\n' f'    "{author}",\n' f"{cells_body}" f"{mounts_body}" "}\n"
    return document, stats


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, default=Path(r"C:/Development/_backup/blueprints"))
    parser.add_argument("--dst", type=Path, default=Path(r"C:/Development/DAQL/assets/Eltanin/blueprints/ships"))
    parser.add_argument("--only", action="append", default=[], help="stem filter (repeatable); default: all")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    args.dst.mkdir(parents=True, exist_ok=True)
    only = {stem.lower() for stem in args.only}
    totals = {
        "files": 0,
        "cells": 0,
        "k8": 0,
        "k7": 0,
        "k6": 0,
        "k4": 0,
        "k4f1111": 0,
        "k3f121": 0,
        "k4f2121": 0,
        "k3f222": 0,
        "wings": 0,
        "dropped_shape": 0,
        "dropped_wing": 0,
        "hangar_skel": 0,
        "dummies": 0,
        "plates": 0,
        "unknown_slot": 0,
        "unknown_plate": 0,
    }

    for path in sorted(args.src.glob("*.blueprint")):
        if only and path.stem.lower() not in only:
            continue
        name, author, volumetrics, wings, plates = parse_legacy(path.read_text(encoding="utf-8"))
        document, stats = convert(name, author, volumetrics, wings, plates)
        out = args.dst / f"{name}.blueprint"
        print(
            f"{path.name} → {out.name}: cells={stats['cells']} "
            f"(k8={stats['k8']} k7={stats['k7']} k6={stats['k6']} k4={stats['k4']} "
            f"flats={stats['wings']}) "
            f"dummies={stats['dummies']} plates={stats['plates']} "
            f"hangar={stats['hangar_skel']} drop_shape={stats['dropped_shape']} drop_wing={stats['dropped_wing']}"
        )
        if stats["unknown_slot"] or stats["unknown_plate"]:
            print(f"  WARN unknown slot={stats['unknown_slot']} plate={stats['unknown_plate']}")
        if not args.dry_run:
            out.write_bytes(document.encode("utf-8"))
        totals["files"] += 1
        for key in totals:
            if key != "files":
                totals[key] += stats[key]

    print("---")
    print(totals)


if __name__ == "__main__":
    main()
