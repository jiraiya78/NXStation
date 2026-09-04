#!/usr/bin/env python3
"""Composite a Switch screenshot into the hero mockup frame."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT_PATH = ASSETS / "hero-mockup.png"

# Default source: user-provided Switch screenshot (copy if present).
DEFAULT_SOURCES = [
    ASSETS / "screenshot-source.png",
    Path(__file__).resolve().parents[2]
    / "assets"
    / "c__Users_Aladdin_AppData_Roaming_Cursor_User_workspaceStorage_daf914abd2677f1d657a0d1839eaeecf_images_2025010308242200-19BCBAA8FF91A5C3F7072B1E8C7D2547-285fc7cc-13ea-4e60-8b19-040fc1d0793f.png",
]

W, H = 640, 400

# Screen inset (matches hero-mockup.svg)
SCREEN_X, SCREEN_Y = 118, 74
SCREEN_W, SCREEN_H = 404, 252
BEZEL_X, BEZEL_Y = 112, 68
BEZEL_W, BEZEL_H = 416, 264


def find_source() -> Path:
    for path in DEFAULT_SOURCES:
        if path.is_file():
            return path
    raise FileNotFoundError(
        "No screenshot source found. Place screenshot-source.png in website/assets/"
    )


def lerp_color(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(int(ac + (bc - ac) * t) for ac, bc in zip(a, b))


def vertical_gradient(size: tuple[int, int], top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    w, h = size
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        c = lerp_color(top, bottom, y / max(h - 1, 1))
        for x in range(w):
            px[x, y] = c
    return img


def rounded_rect_mask(size: tuple[int, int], radius: int) -> Image.Image:
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return mask


def fit_cover(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    src_w, src_h = img.size
    scale = max(target_w / src_w, target_h / src_h)
    new_w = max(1, int(math.ceil(src_w * scale)))
    new_h = max(1, int(math.ceil(src_h * scale)))
    resized = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
    left = (new_w - target_w) // 2
    top = (new_h - target_h) // 2
    return resized.crop((left, top, left + target_w, top + target_h))


def draw_joycon_left(draw: ImageDraw.ImageDraw, base: Image.Image) -> None:
    # Red gradient slab (x 40–124, full height of body)
    joy = vertical_gradient((84, 304), (255, 26, 85), (176, 16, 64))
    mask = Image.new("L", (84, 304), 0)
    mdraw = ImageDraw.Draw(mask)
    mdraw.rounded_rectangle((0, 0, 83, 303), radius=28, fill=255)
    mdraw.rectangle((56, 0, 83, 303), fill=255)
    base.paste(joy, (40, 48), mask)

    # Stick
    draw.ellipse((58, 136, 86, 164), fill=(42, 10, 20))
    draw.ellipse((65, 143, 79, 157), fill=(10, 12, 20))

    # D-pad
    for box in [(54, 210, 66, 222), (66, 222, 78, 234), (78, 222, 90, 234), (66, 234, 78, 246)]:
        draw.rounded_rectangle(box, radius=2, fill=(10, 12, 20))


def draw_joycon_right(draw: ImageDraw.ImageDraw, base: Image.Image) -> None:
    joy = vertical_gradient((84, 304), (0, 240, 255), (0, 144, 168))
    mask = Image.new("L", (84, 304), 0)
    mdraw = ImageDraw.Draw(mask)
    mdraw.rounded_rectangle((0, 0, 83, 303), radius=28, fill=255)
    mdraw.rectangle((0, 0, 27, 303), fill=255)
    base.paste(joy, (536, 48), mask)

    # Minus
    draw.ellipse((562, 124, 574, 136), fill=(10, 12, 20))

    # Face buttons
    draw.ellipse((549, 201, 559, 211), fill=(10, 12, 20))
    draw.ellipse((573, 201, 583, 211), fill=(10, 12, 20))
    draw.ellipse((561, 189, 571, 199), fill=(10, 12, 20))
    draw.ellipse((561, 213, 571, 223), fill=(10, 12, 20))

    # Home
    draw.ellipse((556, 258, 580, 282), fill=(10, 12, 20))


def build_mockup(screenshot: Image.Image) -> Image.Image:
    base = Image.new("RGB", (W, H), (12, 6, 18))

    # Console shell
    draw = ImageDraw.Draw(base)
    draw.rounded_rectangle((40, 48, 599, 351), radius=28, fill=(26, 30, 44))
    draw.rounded_rectangle((40, 48, 599, 351), radius=28, outline=(255, 255, 255, 30), width=1)

    draw_joycon_left(draw, base)
    draw_joycon_right(draw, base)

    # Bezel + screen
    draw = ImageDraw.Draw(base)
    draw.rounded_rectangle((BEZEL_X, BEZEL_Y, BEZEL_X + BEZEL_W, BEZEL_Y + BEZEL_H), radius=8, fill=(5, 7, 14))

    screen = fit_cover(screenshot.convert("RGB"), SCREEN_W, SCREEN_H)
    screen_mask = rounded_rect_mask((SCREEN_W, SCREEN_H), 4)
    base.paste(screen, (SCREEN_X, SCREEN_Y), screen_mask)

    # Subtle screen glare
    glare = Image.new("RGBA", (SCREEN_W, SCREEN_H), (255, 255, 255, 0))
    gdraw = ImageDraw.Draw(glare)
    gdraw.polygon([(0, 0), (SCREEN_W, 0), (0, SCREEN_H)], fill=(255, 255, 255, 18))
    base.paste(glare, (SCREEN_X, SCREEN_Y), glare)

    return base


def main() -> None:
    source = find_source()
    screenshot = Image.open(source)
    mockup = build_mockup(screenshot)
    ASSETS.mkdir(parents=True, exist_ok=True)
    mockup.save(OUT_PATH, format="PNG", optimize=True)
    print(f"Wrote {OUT_PATH} ({mockup.size[0]}x{mockup.size[1]}) from {source}")


if __name__ == "__main__":
    main()
