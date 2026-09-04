#!/usr/bin/env python3
"""Generate personality / playtime metric icons for romfs."""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "resources", "img", "metrics")
SIZE = 128

# id, accent RGB, simple glyph pattern name
ICONS = [
    ("16bit_purist", (120, 88, 220), "sixteen"),
    ("8bit_purist", (196, 58, 58), "eight"),
    ("8bit_pioneer", (196, 58, 58), "eight"),
    ("polygon_crusader", (72, 168, 120), "poly"),
    ("arcade_junkie", (240, 180, 48), "arcade"),
    ("serial_sampler", (96, 168, 208), "grid"),
    ("completionist", (255, 210, 80), "star"),
    ("jrpg_scholar", (140, 96, 200), "book"),
    ("renaissance", (220, 120, 160), "rainbow"),
    ("micro_burst", (255, 140, 100), "burst"),
    ("casual", (120, 180, 255), "casual"),
    ("deep_dive", (80, 140, 220), "dive"),
    ("marathon", (200, 80, 120), "marathon"),
    ("session_style", (100, 180, 200), "clock"),
    ("time_of_day", (255, 200, 100), "sunmoon"),
    ("heatmap", (80, 200, 120), "heatmap"),
    ("backlog_dust", (160, 140, 120), "dust"),
    ("overview", (140, 160, 220), "chart"),
    ("timewarp", (180, 120, 255), "warp"),
    ("sources", (120, 120, 140), "folder"),
    ("note", (140, 140, 160), "info"),
    ("empty", (100, 100, 110), "empty"),
]


def write_png(path, w, h, rgba_fn):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    rows = [b"\x00" + b"".join(bytes(rgba_fn(x, y)) for x in range(w)) for y in range(h)]
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def draw_glyph(kind, x, y, cx, cy, r, accent):
    """Return True if pixel inside icon glyph."""
    dx, dy = x - cx, y - cy
    if kind == "sixteen":
        return abs(dx) < r * 0.75 and abs(dy) < r * 0.55 and (abs(dx) > r * 0.35 or abs(dy) > r * 0.25)
    if kind == "eight":
        return abs(dx) < r * 0.7 and abs(dy) < r * 0.5
    if kind == "poly":
        return dy > -r * 0.2 and dy < r * 0.5 and abs(dx) < r * 0.6 - abs(dy) * 0.4
    if kind == "arcade":
        return (dx * dx + (dy + r * 0.2) ** 2) < (r * 0.35) ** 2 or (
            abs(dx) < r * 0.15 and dy > -r * 0.1 and dy < r * 0.55
        )
    if kind == "grid":
        gx, gy = (x * 4) // SIZE, (y * 4) // SIZE
        return (gx + gy) % 3 != 0 and 20 < x < SIZE - 20 and 20 < y < SIZE - 20
    if kind == "star":
        ang = __import__("math").atan2(dy, dx)
        dist = (dx * dx + dy * dy) ** 0.5
        return dist < r * (0.45 + 0.2 * __import__("math").cos(5 * ang))
    if kind == "book":
        return abs(dx) < r * 0.55 and abs(dy) < r * 0.65 and dx < r * 0.1
    if kind == "rainbow":
        band = int((dy + r) / (2 * r) * 4)
        return abs(dx) < r * 0.7 and -r < dy < r and band % 2 == 0
    if kind == "burst":
        return (dx * dx + dy * dy) < (r * 0.25) ** 2 or (
            abs(dx) < r * 0.12 and abs(dy) < r * 0.55
        ) or (abs(dy) < r * 0.12 and abs(dx) < r * 0.55)
    if kind == "casual":
        return (dx * dx + dy * dy) < (r * 0.45) ** 2
    if kind == "dive":
        return dy > -r * 0.5 and dy < r * 0.2 and abs(dx) < r * 0.5 - dy * 0.3
    if kind == "marathon":
        return abs(dx) < r * 0.55 and abs(dy - (abs(dx) * 0.4)) < r * 0.12
    if kind == "clock":
        return (dx * dx + dy * dy) < (r * 0.55) ** 2 and not (
            (dx * dx + dy * dy) < (r * 0.08) ** 2
        )
    if kind == "sunmoon":
        return (dx - r * 0.25) ** 2 + dy ** 2 < (r * 0.35) ** 2 or (
            (dx + r * 0.25) ** 2 + dy ** 2 < (r * 0.3) ** 2 and dx > -r * 0.1
        )
    if kind == "heatmap":
        return 16 < x < SIZE - 16 and 16 < y < SIZE - 16 and ((x // 16) + (y // 16)) % 3 != 1
    if kind == "dust":
        return (dx * dx + dy * dy) < (r * 0.5) ** 2 and (x + y) % 7 < 3
    if kind == "chart":
        return 20 < x < SIZE - 20 and SIZE - 20 - (x % 24) * 2 < y < SIZE - 24
    if kind == "warp":
        return abs(dx) < r * 0.15 and abs(dy) < r * 0.6 or abs(dy) < r * 0.15 and abs(dx) < r * 0.6
    if kind == "folder":
        return abs(dx) < r * 0.6 and abs(dy) < r * 0.45 and dy > -r * 0.15
    if kind == "info":
        return (dx * dx + dy * dy) < (r * 0.5) ** 2
    if kind == "empty":
        return abs(abs(dx) - abs(dy)) < r * 0.08 and abs(dx) < r * 0.45
    return False


def make_icon(accent, kind):
    cx = cy = SIZE // 2
    r = SIZE * 0.38
    bg = (28, 30, 42, 255)
    ring = accent + (255,)

    def pixel(x, y):
        dx, dy = x - cx, y - cy
        dist = (dx * dx + dy * dy) ** 0.5
        if dist > r + 4:
            return bg
        if dist > r - 2:
            return ring
        if draw_glyph(kind, x, y, cx, cy, r, accent):
            return accent + (255,)
        return (40, 42, 58, 255)

    return pixel


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for icon_id, accent, kind in ICONS:
        write_png(os.path.join(OUT_DIR, f"{icon_id}.png"), SIZE, SIZE, make_icon(accent, kind))
    print(f"wrote {len(ICONS)} metric icons to {OUT_DIR}")


if __name__ == "__main__":
    main()
