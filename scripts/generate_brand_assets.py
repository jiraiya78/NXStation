#!/usr/bin/env python3
"""Verify bundled brand placeholder assets exist (user-provided, not generated)."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG_DIR = os.path.join(ROOT, "resources", "img")
SYS_DIR = os.path.join(ROOT, "resources", "img", "systems")
BG_DIR = os.path.join(ROOT, "resources", "img", "background")


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    os.makedirs(SYS_DIR, exist_ok=True)
    os.makedirs(BG_DIR, exist_ok=True)

    system_path = os.path.join(IMG_DIR, "nxstation.jpg")
    if os.path.isfile(system_path):
        print(f"kept existing {system_path}")
    else:
        print(f"warning: missing {system_path} — add your system carousel placeholder")

    box_path = os.path.join(SYS_DIR, "nxstation_box.png")
    if os.path.isfile(box_path):
        print(f"kept existing {box_path}")
    else:
        print(f"warning: missing {box_path} — add your game box placeholder")

    for name in ("favorite", "lastplayed"):
        bg_path = os.path.join(BG_DIR, f"{name}.jpg")
        if os.path.isfile(bg_path):
            print(f"kept existing {bg_path}")
        else:
            print(f"warning: missing {bg_path} — add carousel background for {name}")


if __name__ == "__main__":
    main()
