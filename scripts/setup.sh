#!/usr/bin/env bash
# Bootstrap SwitchFrontend dependencies
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p library third_party/nlohmann resources/img

if [[ ! -f library/borealis/library/cmake/commonOption.cmake ]]; then
  echo "==> Cloning xfangfang/borealis"
  if [[ -d library/borealis/.git ]]; then
    git -C library/borealis submodule update --init --recursive
  else
    git clone --recursive https://github.com/xfangfang/borealis.git library/borealis
  fi
fi

echo "==> Fetching optional third_party headers"
if [[ ! -f third_party/nlohmann/json.hpp ]]; then
  curl -fsSL -o third_party/nlohmann/json.hpp \
    https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp || \
    echo "warning: could not download nlohmann/json (built-in sf::Json will be used)"
fi

if [[ ! -f third_party/stb_image.h ]]; then
  curl -fsSL -o third_party/stb_image.h \
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h || \
    echo "warning: could not download stb_image (Borealis image loader will be used)"
fi

cp -f roms_config.json resources/roms_config.json

if [[ ! -f resources/img/placeholder.png ]]; then
  echo "==> Generating placeholder images"
  python3 - <<'PY' 2>/dev/null || python - <<'PY'
import struct, zlib, os
os.makedirs('resources/img', exist_ok=True)

def write_png(path, w, h, rgba_fn):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    rows = []
    for y in range(h):
        row = b'\x00'
        for x in range(w):
            row += bytes(rgba_fn(x, y))
        rows.append(row)
    raw = b''.join(rows)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDATA', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)

write_png('resources/img/placeholder.png', 320, 280, lambda x,y: (48 + (x//32)*3, 52 + (y//32)*3, 64, 255))
write_png('resources/img/icon.png', 256, 256, lambda x,y: (30 + x*80//255, 120 + y*60//255, 200 - x*40//255, 255))
PY
fi

if [[ -d library/borealis/resources ]]; then
  echo "==> Merging Borealis base resources (fonts, styles)"
  # Copy missing base assets without clobbering our XML/img
  cp -rn library/borealis/resources/. resources/ 2>/dev/null || \
  (cd library/borealis/resources && tar cf - . | (cd "$ROOT/resources" && tar xf - --skip-old-files))
fi

echo "==> Done. Configure with:"
echo "    cmake -B build_switch -DPLATFORM_SWITCH=ON"
echo "    cmake --build build_switch --target SwitchFrontend.nro"
