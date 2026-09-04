#!/usr/bin/env python3
"""Resolve addresses from a crash report to source locations in NXStation.elf.

Accepts either the crash.log written by NXStation's own exception handler (which
already reports "elf 0x..." offsets) or an Atmosphere crash report, where the
per-module offsets appear in the backtrace section.

Usage (PowerShell):
    python scripts/symbolize_crash.py log/crash.log
    python scripts/symbolize_crash.py --module-base 0x8004000 log/report.log

Requires docker with the devkitpro/devkita64 image, which supplies addr2line.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_ELF = REPO / "build_switch" / "NXStation.elf"
ADDR2LINE = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line"

# "pc = 0x... (elf 0x1234)" from our handler, or a bare hex address per line.
ELF_OFFSET = re.compile(r"elf\s+0x([0-9a-fA-F]+)")
BARE_ADDR = re.compile(r"\b0x([0-9a-fA-F]{4,16})\b")


def collect_offsets(text: str, module_base: int | None) -> list[int]:
    offsets = [int(m, 16) for m in ELF_OFFSET.findall(text)]
    if offsets:
        return offsets
    if module_base is None:
        return []
    raw = [int(m, 16) for m in BARE_ADDR.findall(text)]
    return [a - module_base for a in raw if a >= module_base]


def symbolize(elf: Path, offsets: list[int]) -> str:
    args = [
        "docker", "run", "--rm",
        "-v", f"{REPO}:/src",
        "--workdir", "/src",
        "devkitpro/devkita64",
        ADDR2LINE, "-Cfpie", str(elf.relative_to(REPO)).replace("\\", "/"),
        *[hex(o) for o in offsets],
    ]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"addr2line failed:\n{result.stderr}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, help="crash.log or Atmosphere report")
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument(
        "--module-base",
        type=lambda v: int(v, 0),
        help="Runtime module base, needed only for reports without 'elf 0x...' offsets",
    )
    args = parser.parse_args()

    if not args.elf.exists():
        sys.exit(f"{args.elf} not found — build first so symbols match the crashing binary")

    offsets = collect_offsets(args.report.read_text(errors="replace"), args.module_base)
    if not offsets:
        sys.exit("No addresses found. Pass --module-base for an Atmosphere report.")

    print(symbolize(args.elf, offsets))


if __name__ == "__main__":
    main()
