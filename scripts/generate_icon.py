#!/usr/bin/env python3
"""Generate NXStation Switch NRO icon (256x256 JPEG required by elf2nro)."""
import os
import struct
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG_DIR = os.path.join(ROOT, "resources", "img")
SOURCE_PNG = os.path.join(IMG_DIR, "icon_source.png")
OUT_PNG = os.path.join(IMG_DIR, "icon.png")
OUT_JPG = os.path.join(IMG_DIR, "icon.jpg")
ICON_CONVERT_C = os.path.join(ROOT, "scripts", "icon_convert.c")
ICON_CONVERT_BIN = os.path.join(ROOT, "scripts", "icon_convert")


def write_png(path, w, h, rgba_fn):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    rows = [b"\x00" + b"".join(bytes(rgba_fn(x, y)) for x in range(w)) for y in range(h)]
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def in_rect(px, py, rx, ry, rw, rh):
    return rx <= px < rx + rw and ry <= py < ry + rh


def in_circle(px, py, cx, cy, r):
    return (px - cx) ** 2 + (py - cy) ** 2 <= r * r


def pixel(x, y):
    bg = (18, 19, 26, 255)
    if in_rect(x, y, 48, 88, 160, 88):
        return (45, 48, 68, 255)
    if in_rect(x, y, 24, 108, 36, 52) or in_rect(x, y, 196, 108, 36, 52):
        return (45, 48, 68, 255)
    if in_rect(x, y, 78, 118, 14, 44) or in_rect(x, y, 68, 132, 34, 14):
        return (79, 82, 220, 255)
    for bx, by, c in (
        (158, 122, (99, 102, 241)),
        (182, 142, (129, 140, 248)),
        (158, 162, (99, 102, 241)),
        (134, 142, (99, 102, 241)),
    ):
        if in_circle(x, y, bx, by, 11):
            return c + (255,)
    return bg


def build_icon_convert():
    need_build = not os.path.isfile(ICON_CONVERT_BIN)
    if not need_build and os.path.isfile(ICON_CONVERT_C):
        need_build = os.path.getmtime(ICON_CONVERT_C) > os.path.getmtime(ICON_CONVERT_BIN)
    if not need_build:
        return True
    try:
        subprocess.check_call(
            ["gcc", "-O2", "-o", ICON_CONVERT_BIN, ICON_CONVERT_C, "-lm"],
            cwd=ROOT,
        )
        return True
    except (OSError, subprocess.CalledProcessError) as ex:
        print(f"generate_icon: could not build icon_convert: {ex}", file=sys.stderr)
        return False


def from_icon_source():
    if not os.path.isfile(SOURCE_PNG):
        return False

    os.makedirs(IMG_DIR, exist_ok=True)
    if not build_icon_convert():
        return False

    try:
        subprocess.check_call([ICON_CONVERT_BIN, SOURCE_PNG, OUT_PNG, OUT_JPG])
    except (OSError, subprocess.CalledProcessError) as ex:
        print(f"generate_icon: failed to process {SOURCE_PNG}: {ex}", file=sys.stderr)
        return False

    if not os.path.isfile(OUT_JPG) or os.path.getsize(OUT_JPG) < 1024:
        print(f"generate_icon: {OUT_JPG} missing or too small", file=sys.stderr)
        return False

    print(f"generate_icon: using {SOURCE_PNG}")
    print("wrote", OUT_PNG)
    print("wrote", OUT_JPG)
    return True


def from_placeholder():
    write_png(OUT_PNG, 256, 256, pixel)
    if not build_icon_convert():
        print("generate_icon: no icon_source.png and icon_convert unavailable", file=sys.stderr)
        sys.exit(1)
    try:
        subprocess.check_call([ICON_CONVERT_BIN, OUT_PNG, OUT_PNG, OUT_JPG])
    except (OSError, subprocess.CalledProcessError):
        print("generate_icon: placeholder JPEG conversion failed", file=sys.stderr)
        sys.exit(1)
    print("generate_icon: using built-in placeholder (no icon_source.png)")
    print("wrote", OUT_JPG)


def main():
    if not from_icon_source():
        from_placeholder()


if __name__ == "__main__":
    main()
