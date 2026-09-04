#!/usr/bin/env python3
"""Fetch Nunito rounded font for romfs (optional; build continues if offline)."""
import os
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "resources", "font", "font.ttf")
URLS = [
    "https://github.com/googlefonts/nunito/raw/refs/heads/main/fonts/ttf/Nunito-Regular.ttf",
    "https://raw.githubusercontent.com/googlefonts/nunito/main/fonts/ttf/Nunito-Regular.ttf",
]


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    if os.path.isfile(OUT) and os.path.getsize(OUT) > 10000:
        print("font already present:", OUT)
        return

    for url in URLS:
        try:
            urllib.request.urlretrieve(url, OUT)
            if os.path.getsize(OUT) > 10000:
                print("downloaded", OUT)
                return
        except Exception as ex:
            print("fetch attempt failed:", url, ex, file=sys.stderr)

    borealis_font = os.path.join(ROOT, "library", "borealis", "resources", "font", "switch_font.ttf")
    if os.path.isfile(borealis_font):
        with open(borealis_font, "rb") as src, open(OUT, "wb") as dst:
            dst.write(src.read())
        print("copied borealis fallback font to", OUT)
        return

    print("fetch_round_font: no font available — using borealis default", file=sys.stderr)


if __name__ == "__main__":
    main()
