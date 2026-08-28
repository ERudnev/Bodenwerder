#!/usr/bin/env python3
"""Lunar-style grayscale damage mask: white field eaten by overlapping craters."""

from __future__ import annotations

import math
import random
import struct
from pathlib import Path

SIZE = 1024
SEED = 20260829
CRATERS = 220
PITS = 900


def write_bmp_gray(path: Path, size: int, pixels: list[float]) -> None:
    row_stride = (size * 3 + 3) & ~3
    pixel_bytes = row_stride * size
    header = 14 + 40
    with path.open("wb") as out:
        out.write(b"BM")
        out.write(struct.pack("<IHHI", header + pixel_bytes, 0, 0, header))
        out.write(struct.pack("<IiiHHIIiiII", 40, size, size, 1, 24, 0, pixel_bytes, 2835, 2835, 0, 0))
        pad = b"\x00" * (row_stride - size * 3)
        for y in range(size - 1, -1, -1):
            row = bytearray()
            base = y * size
            for x in range(size):
                tone = max(0, min(255, int(pixels[base + x] * 255.0 + 0.5)))
                row.extend((tone, tone, tone))
            out.write(row)
            if pad:
                out.write(pad)


def stamp_bowl(height: list[float], cx: float, cy: float, radius: float, depth: float) -> None:
    reach = radius * 1.22
    x0 = max(0, int(cx - reach))
    x1 = min(SIZE, int(cx + reach) + 1)
    y0 = max(0, int(cy - reach))
    y1 = min(SIZE, int(cy + reach) + 1)
    for y in range(y0, y1):
        for x in range(x0, x1):
            dx = (x + 0.5) - cx
            dy = (y + 0.5) - cy
            n = math.hypot(dx, dy) / radius
            i = y * SIZE + x
            if n < 1.0:
                bowl = (1.0 - n * n) ** 1.15
                height[i] -= depth * bowl
            elif n < 1.2:
                rim = max(0.0, 1.0 - abs(n - 1.08) / 0.12)
                height[i] += depth * 0.16 * rim


def main() -> None:
    rng = random.Random(SEED)
    height = [1.0] * (SIZE * SIZE)
    for _ in range(CRATERS):
        stamp_bowl(
            height,
            rng.uniform(0.0, float(SIZE)),
            rng.uniform(0.0, float(SIZE)),
            rng.uniform(12.0, 160.0),
            rng.uniform(0.07, 0.52),
        )
    for _ in range(PITS):
        stamp_bowl(
            height,
            rng.uniform(0.0, float(SIZE)),
            rng.uniform(0.0, float(SIZE)),
            rng.uniform(2.0, 14.0),
            rng.uniform(0.04, 0.22),
        )
    out = []
    for value in height:
        out.append(max(0.02, min(0.97, value)))
    target = Path(__file__).with_name("damage_mask.bmp")
    write_bmp_gray(target, SIZE, out)
    print(f"wrote {target} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
