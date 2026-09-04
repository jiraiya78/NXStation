#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MU="${ROOT}/third_party/mupdf"
BUILD="${MU}/build/release"

source /opt/devkitpro/switchvars.sh

if [[ ! -d "${MU}/.git" ]]; then
    git clone --depth 1 --branch 1.24.10 https://github.com/ArtifexSoftware/mupdf.git "${MU}"
    cd "${MU}"
    git submodule update --init --depth 1
else
    cd "${MU}"
fi

# newlib on Switch has no timegm(); PDF metadata dates only need a stable helper.
sed -i 's/timegm(&tm)/mktime(\&tm)/' source/pdf/pdf-parse.c

export CC="${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-gcc"
export CXX="${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-g++"
export AR="${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-ar"
export RANLIB="${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-ranlib"
COMPAT="-include ${ROOT}/third_party/mupdf_switch_compat.h"

make -j"$(nproc)" build=release generate
rm -f "${BUILD}/source/pdf/pdf-parse.o"
make -j"$(nproc)" build=release libs \
    HAVE_X11=no HAVE_GLUT=no HAVE_OBJCOPY=no \
    USE_SYSTEM_LIBS=no

echo "MuPDF built: ${BUILD}/libmupdf.a"
