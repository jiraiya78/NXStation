#!/usr/bin/env python3
"""Embed nx-hbloader exefs blobs for on-device forwarder generation."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXEFS = os.path.join(ROOT, "third_party", "forwarder", "exefs")
OUT = os.path.join(ROOT, "source", "forwarder", "HblEmbed.cpp")


def emit_array(name: str, path: str, out_lines: list[str]) -> None:
    with open(path, "rb") as f:
        data = f.read()
    out_lines.append(f"alignas(4) extern const unsigned char {name}[] = {{")
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hexes = ", ".join(f"0x{b:02x}" for b in chunk)
        out_lines.append(f"    {hexes},")
    out_lines.append("};")
    out_lines.append(f"extern const unsigned int {name}_size = {len(data)};")
    out_lines.append("")


def main() -> int:
    main_path = os.path.join(EXEFS, "main")
    npdm_path = os.path.join(EXEFS, "main.npdm")
    if not os.path.isfile(main_path) or not os.path.isfile(npdm_path):
        print("Missing hbl exefs — build sphaira/hbl or place main + main.npdm in third_party/forwarder/exefs/")
        return 1

    lines = [
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace sf::forwarder {",
        "",
    ]
    emit_array("kHblMain", main_path, lines)
    emit_array("kHblNpdm", npdm_path, lines)
    lines.append("} // namespace sf::forwarder")
    lines.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"Wrote {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
