"""Convert RedStar construction ship *.json → Eltanin *.blueprint.

Smoke format (positional, meshpack-style):
  { name, author, cells[], stubs[], hull[] }
  cell  = [[x,y,z], ori, frame_shape, inner_role, volume, align]
  stub  = [[x,y,z], ori, wing_shape, wing_role]
  plate = [[x,y,z], ori, plate_shape, plate_role]
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

FRAME_SHAPES = {0: "k8", 1: "k7", 2: "k6", 3: "k4"}
PLATE_SHAPES = {0: "p1111", 1: "p121", 2: "p2121", 3: "p222A", 4: "p222V"}
WING_SHAPES = {0: "w1111", 1: "w121", 2: "w2121", 3: "w321", 4: "w222"}
# RedStar CubeModule::Type
INNER_ROLES = {
    0: "engine",
    1: "power",
    2: "gyros",
    3: "control",
    4: "living",
    5: "cargo",
    6: "hardpoint",
    7: "hangar",
    8: "multi",
    9: "multi",  # TANK → multi (no tank role yet)
}

DEFAULT_AUTHOR = "Concordia"
CLASS_PREFIX = re.compile(r"^[mc]\d+-(.+)$", re.IGNORECASE)


def blueprint_name(stem: str) -> str:
    match = CLASS_PREFIX.match(stem)
    if match:
        return match.group(1)
    return stem.lstrip("_")


def convert(data: dict, name: str, author: str) -> tuple[list, list, list]:
    cells: list = []
    stubs: list = []
    hull: list = []

    for cube in data.get("cubes", []):
        pos = cube["position"]
        px, py, pz = pos["x"], pos["y"], pos["z"]
        cells.append([
            [px, py, pz],
            int(cube["transform"]),
            FRAME_SHAPES[int(cube["topology"])],
            INNER_ROLES[int(cube["module"]["type"])],
            "full",
            0,
        ])
        for plate in cube.get("plates", []):
            if plate.get("empty"):
                continue
            hull.append([
                [px, py, pz],
                int(plate["transform"]),
                PLATE_SHAPES[int(plate["topology"])],
                "armor",
            ])

    for wing in data.get("wings", []):
        pos = wing["position"]
        stubs.append([
            [pos["x"], pos["y"], pos["z"]],
            int(wing["transform"]),
            WING_SHAPES[int(wing["topology"])],
            "radiance",
        ])

    return cells, stubs, hull


def fmt_row(row: list, indent: str = "        ") -> str:
    return indent + json.dumps(row, separators=(", ", ": "))


def write_array(lines: list[str], rows: list, trailing_comma: bool) -> None:
    if not rows:
        lines.append("    []" + ("," if trailing_comma else ""))
        return
    lines.append("    [")
    for i, row in enumerate(rows):
        comma = "," if i + 1 < len(rows) else ""
        lines.append(fmt_row(row) + comma)
    lines.append("    ]" + ("," if trailing_comma else ""))


def write_blueprint(path: Path, name: str, author: str, cells: list, stubs: list, hull: list) -> None:
    lines = ["{", f'    "{name}",', f'    "{author}",']
    write_array(lines, cells, trailing_comma=True)
    write_array(lines, stubs, trailing_comma=True)
    write_array(lines, hull, trailing_comma=False)
    lines.append("}")
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    here = Path(__file__).resolve()
    daql = here.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--src",
        type=Path,
        default=Path(r"c:/Development/Engine/RedStar/Resources/construction/ships"),
        help="RedStar ships directory",
    )
    parser.add_argument(
        "--dst",
        type=Path,
        default=daql / "assets" / "Eltanin" / "blueprints",
        help="Output blueprints directory",
    )
    parser.add_argument("--author", default=DEFAULT_AUTHOR)
    parser.add_argument(
        "--skip-underscore",
        action="store_true",
        help="Skip debug fixtures like _arr.json / _coolingDebug.json",
    )
    args = parser.parse_args()

    sources = sorted(args.src.glob("*.json"))
    if not sources:
        raise SystemExit(f"no *.json in {args.src}")

    wrote = 0
    for src in sources:
        if args.skip_underscore and src.name.startswith("_"):
            print(f"skip {src.name}")
            continue
        name = blueprint_name(src.stem)
        data = json.loads(src.read_text(encoding="utf-8"))
        cells, stubs, hull = convert(data, name, args.author)
        dst = args.dst / f"{name}.blueprint"
        write_blueprint(dst, name, args.author, cells, stubs, hull)
        print(f"{src.name} → {dst.name}  cells={len(cells)} stubs={len(stubs)} hull={len(hull)}")
        wrote += 1

    print(f"wrote {wrote} blueprint(s) → {args.dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
