# Optional PowerShell bootstrap for Windows hosts
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Resolve-Git {
    $cmd = Get-Command git -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        "C:\Program Files\Git\cmd\git.exe",
        "C:\Program Files\Git\bin\git.exe",
        "C:\Program Files (x86)\Git\cmd\git.exe",
        "$env:LOCALAPPDATA\Programs\Git\cmd\git.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

$Git = Resolve-Git
if (-not $Git) {
    Write-Warning "Git not found in PATH. Install Git for Windows or add it to PATH."
}

New-Item -ItemType Directory -Force -Path library, third_party/nlohmann, resources/img | Out-Null

if (-not (Test-Path "library/borealis/library/cmake/commonOption.cmake")) {
    if (-not $Git) {
        throw "Borealis missing and Git unavailable. Clone manually: git clone --recursive https://github.com/xfangfang/borealis.git library/borealis"
    }
    Write-Host "==> Cloning xfangfang/borealis"
    & $Git clone --recursive https://github.com/xfangfang/borealis.git library/borealis
} elseif ($Git) {
    Write-Host "==> Updating borealis submodules"
    & $Git -C library/borealis submodule update --init --recursive
}

function Try-Download($Url, $Out) {
    if (Test-Path $Out) { return }
    try {
        Invoke-WebRequest -Uri $Url -OutFile $Out -UseBasicParsing
        Write-Host "downloaded $Out"
    } catch {
        Write-Warning "could not download $Out (optional)"
    }
}

Try-Download "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" "third_party/nlohmann/json.hpp"
Try-Download "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" "third_party/stb_image.h"

if (-not (Test-Path "resources/img/placeholder.png")) {
    python -c @"
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
"@
}

Copy-Item roms_config.json resources/roms_config.json -Force

if (Test-Path "library/borealis/resources") {
    Write-Host "==> Merging Borealis base resources"
    Copy-Item -Path "library/borealis/resources/*" -Destination "resources/" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Done. Build on a machine with devkitPro + CMake."
