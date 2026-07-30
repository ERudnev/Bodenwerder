import math
import struct
import zlib
from pathlib import Path

# Single star flare sprite — nothing else.
W = H = 5


def clamp(x, a=0.0, b=1.0):
    return a if x < a else b if x > b else x


def write_png(path, rgba, w, h):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw.extend(rgba[y * w * 4 : (y + 1) * w * 4])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    Path(path).write_bytes(data)


px = bytearray(W * H * 4)


def setp(x, y, r, g, b, a):
    if 0 <= x < W and 0 <= y < H:
        i = (y * W + x) * 4
        px[i] = max(px[i], int(clamp(r) * 255))
        px[i + 1] = max(px[i + 1], int(clamp(g) * 255))
        px[i + 2] = max(px[i + 2], int(clamp(b) * 255))
        px[i + 3] = max(px[i + 3], int(clamp(a) * 255))


for y in range(5):
    for x in range(5):
        dx = x + 0.5 - 2.5
        dy = y + 0.5 - 2.5
        r = math.hypot(dx, dy)
        glow = math.exp(-(r * r) / (2 * 1.35 * 1.35))
        setp(x, y, glow, glow, glow, glow)

out = Path(r"c:\Development\DAQL\assets\Kubes\sprites\skySphere.png")
write_png(out, px, W, H)
print("wrote", out, "bytes", out.stat().st_size)
