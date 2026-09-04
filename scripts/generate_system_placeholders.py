#!/usr/bin/env python3
"""Generate per-system box-art placeholder images for romfs."""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "resources", "img", "systems")

# id, label, accent RGB
SYSTEMS = [
    ("nes", "NES", (196, 58, 58)),
    ("snes", "SNES", (88, 92, 220)),
    ("n64", "N64", (58, 160, 88)),
    ("gba", "GBA", (72, 88, 196)),
    ("gb", "GB", (96, 96, 112)),
    ("gbc", "GBC", (72, 168, 168)),
    ("nds", "NDS", (52, 52, 64)),
    ("megadrive", "MEGADRIVE", (72, 120, 196)),
    ("mastersystem", "MASTER SYSTEM", (52, 88, 148)),
    ("gamegear", "GAME GEAR", (96, 72, 148)),
    ("psx", "PSX", (88, 88, 140)),
    ("ps2", "PS2", (48, 56, 108)),
    ("psp", "PSP", (64, 64, 96)),
    ("pce", "PCE", (196, 104, 48)),
    ("atari2600", "ATARI 2600", (168, 96, 40)),
    ("cps1", "CPS1", (176, 64, 96)),
    ("cps2", "CPS2", (140, 60, 152)),
    ("neogeo", "NEO GEO", (208, 132, 40)),
    ("dreamcast", "DREAMCAST", (216, 128, 64)),
    ("3ds", "3DS", (72, 148, 168)),
]

W, H = 400, 560


def write_png(path, w, h, rgba_fn):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    rows = [b"\x00" + b"".join(bytes(rgba_fn(x, y)) for x in range(w)) for y in range(h)]
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def draw_label_bitmap(label):
    glyphs = {
        " ": ["....."] * 7,
        "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
        "1": [".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
        "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
        "3": [".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."],
        "4": ["#...#", "#...#", "#...#", "#####", "....#", "....#", "....#"],
        "5": ["#####", "#....", "#....", "####.", "....#", "#...#", ".###."],
        "6": [".###.", "#...#", "#....", "####.", "#...#", "#...#", ".###."],
        "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
        "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
        "9": [".###.", "#...#", "#...#", ".####", "....#", "#...#", ".###."],
        "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
        "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
        "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
        "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
        "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
        "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
        "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
        "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
        "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
        "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
        "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
        "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    }
    text = label.upper()[:12]
    cols = []
    for ch in text:
        cols.append(glyphs.get(ch, glyphs[" "]))
        cols.append(["....."] * 7)
    return cols


def make_pixel_fn(accent, label):
    cols = draw_label_bitmap(label)
    gw = len(cols)
    gh = 7
    scale = 6
    tw = gw * scale
    th = gh * scale
    ox = (W - tw) // 2
    oy = (H - th) // 2
    bg = (22, 23, 31, 255)
    panel = (32, 34, 48, 255)

    def pixel(x, y):
        if 24 <= x < W - 24 and 40 <= y < H - 40:
            base = panel
        else:
            base = bg
        if ox <= x < ox + tw and oy <= y < oy + th:
            cx = (x - ox) // scale
            cy = (y - oy) // scale
            if cols[cx][cy] == "#":
                return accent + (255,)
        if H - 80 <= y < H - 56 and W // 2 - 60 <= x < W // 2 + 60:
            return accent + (200,)
        return base

    return pixel


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for sid, label, accent in SYSTEMS:
        png_path = os.path.join(OUT_DIR, f"{sid}.png")
        write_png(png_path, W, H, make_pixel_fn(accent, label))
    print(f"wrote {len(SYSTEMS)} system placeholders to {OUT_DIR}")


if __name__ == "__main__":
    main()
