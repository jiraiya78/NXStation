# NXStation

Lightweight Nintendo Switch homebrew frontend for browsing ROM libraries, scraping box art / metadata / video previews / PDF manuals, and chain-loading RetroArch or standalone cores via `envSetNextLoad()`.

**Current version:** 1.0.0 — see [`changelog.md`](changelog.md).

**Docs:** [Wiki](https://nxstation.com/wiki.html) · [Blog](https://nxstation.com/blog.html) · [GitHub Releases](https://github.com/jiraiya78/NXStation/releases)

Built against **devkitA64 / libnx**, **Borealis** (UI), **libcurl**, optional **FFmpeg**, and optional **MuPDF** (PDF manuals).

## Features

| Area | Capability |
| --- | --- |
| Library | Recursive ROM scan from `roms_config.json`, virtualized game lists, Favorites + Last Played |
| Scraping | ScreenScraper client, CRC32 hashing, local metadata/artwork/video/manual cache |
| Manuals | PDF download + in-app viewer (MuPDF); cover+spread layout, zoom/pan |
| Playback | Core-direct RetroArch or standalone `.nro` launch (`envSetNextLoad`), hover video preview (FFmpeg), launch zoom/fade |
| UI | System carousel (Fade / Slide / Crossfade / Zoom / None), list-style system browser, Hide Empty Systems, themes, playtime analytics |
| Cloud | Google Drive backup/restore for RetroArch saves & states (Settings → Cloud Save) |
| Updates | Settings → **Check for Updates** via [GitHub Releases](https://github.com/jiraiya78/NXStation/releases) |
| Website | Landing page, user wiki, blog (release notes) in `website/` |

Horizon constraints from the spec are enforced:

- Applet vs Title detection → disables video + high-res cache in Applet Mode
- Main-thread GL / UI only; workers for hash, curl, decode
- LRU texture eviction + flush before `envSetNextLoad`
- Offline-safe scrape fallbacks and Borealis toasts on errors

## Updating

1. In-app: Settings → **Check for Updates** (needs internet). Downloads `NXStation.nro` from the latest GitHub Release asset and replaces `sdmc:/switch/NXStation/NXStation.nro`. Restart afterward.
2. Manual: copy a new `NXStation.nro` over the same path. Settings and data are kept.

Release publishers must attach an asset named exactly **`NXStation.nro`** and use a semver tag (`v1.0.0`).

## Repository layout

```text
include/{app,ui,launcher,scraper,media,util}/
source/...
resources/{xml,img,i18n}/
website/                    ← static site (wiki, blog, landing)
roms_config.json
CMakeLists.txt
PROJECT_SPEC.md
```

## Prerequisites

1. [devkitPro](https://devkitpro.org/wiki/Getting_Started) with Switch packages:
   ```bash
   (dkp-)pacman -S switch-dev switch-glfw switch-mesa switch-glm switch-curl switch-mbedtls
   # optional video:
   (dkp-)pacman -S switch-ffmpeg
   ```
   MuPDF for PDF manuals is built automatically via `scripts/build_mupdf_switch.sh` when `-DSF_ENABLE_MUPDF=ON` (default).
2. Git, CMake ≥ 3.16
3. Borealis submodule (xfangfang fork)

## Setup

```bash
git clone --recurse-submodules https://github.com/jiraiya78/NXStation.git
cd NXStation

# if you already cloned without submodules:
git submodule update --init --recursive
```

```bash
# optional: nlohmann/json + stb_image for richer decode/parse
# curl -L -o third_party/nlohmann/json.hpp https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
# curl -L -o third_party/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

A built-in `sf::Json` parser is used when nlohmann is absent. Image display still works via Borealis `setImageFromFile`; `stb_image` enables the custom GL LRU uploader.

## Build (Switch)

### Docker (recommended on Windows)

```powershell
function dmake { docker run --rm -v ${PWD}:/src --workdir /src devkitpro/devkita64 make $args }

dmake          # builds build_switch/NXStation.nro
dmake clean    # remove build dir
dmake -j8      # parallel build
```

### Native devkitPro

```bash
cmake -B build_switch -DPLATFORM_SWITCH=ON -DSF_ENABLE_FFMPEG=ON -DSF_ENABLE_MUPDF=ON
cmake --build build_switch --target NXStation.nro -j$(nproc)
```

Copy `build_switch/NXStation.nro` to `sdmc:/switch/NXStation/`.

On first launch, reference `roms_config.json` and `user_cores.json` are copied into `sdmc:/switch/NXStation/settings/` (folder IDs, core paths, troubleshooting). Edit those files or use Settings to customize.

## SD card layout

```text
sdmc:/switch/NXStation/NXStation.nro
sdmc:/switch/NXStation/cores/                    ← optional standalone cores (Tico Dolphin/Azahar)
sdmc:/switch/NXStation/settings/roms_config.json   ← add systems here
sdmc:/switch/NXStation/settings/user_cores.json    ← core overrides only
sdmc:/switch/NXStation/settings/user_last_played.json
sdmc:/switch/NXStation/data/{meta,artwork,video,cache,cloud,theme}/
sdmc:/switch/NXStation/settings/system_descriptions.json  ← system list blurbs (edit this)
sdmc:/switch/NXStation/log/cloud.log              ← cloud backup/restore log
sdmc:/switch/retroarch_switch.nro
sdmc:/retroarch/cores/*.nro
sdmc:/roms/{nes,snes,n64,gba,...}/
sdmc:/roms/{systemId}/manuals/{stem}-manual.pdf  ← scraped PDF manuals
```

Default systems (28) include Atari family, Nintendo through Wii/3DS, Sega through Saturn/Dreamcast, arcade (CPS1/2, Neo Geo, FBNeo), PSX, PSP, and PC Engine. **PS2** remains omitted (no practical Switch RetroArch core).

**GameCube / Wii / 3DS** need standalone [Tico](https://github.com/ticohq) cores downloaded to `cores/` and set in Settings → **Core Paths** — see the [wiki](https://nxstation.com/wiki.html#wiki-cores).

**Last Played:** newest first; list rows show system acronym and last-played date/time.

**ScreenScraper:** Users must enter their [screenscraper.fr](https://www.screenscraper.fr) website login in Settings (`ssid` / `sspassword` → `settings/user_screenscraper.json`). Scraping is blocked without it.

**Playtime analytics (Y menu):** Reads RetroArch runtime logs (`sdmc:/retroarch/playlists/logs/**/*.lrtl`) and playlists, merged with NXStation session data (`data/playtime_nxstation.json`). Enable **Settings → Saving → Save runtime log** in RetroArch for full RetroArch-side history. Launch counts and playtime for library launches are tracked by NXStation. Standalone cores (e.g. Tico Dolphin/Azahar) are not tracked.

**Cloud Save (Settings → Cloud Save):** Link Google account, **Backup Saves to Cloud**, optional **Auto Cloud Save** after returning from a game, and **Cloud Restore** from Drive backups. See [`context.md`](context.md) and the [wiki](https://nxstation.com/wiki.html).

**Game manuals:** Enable **Manual** in the scrape menu (off by default), or copy PDFs to `roms/{systemId}/manuals/{stem}-manual.pdf`. Open from **Y → Menu → Show Metadata → Manual**.

### Theme music and sound effects

Drop files into the active theme folder: `sdmc:/switch/NXStation/data/theme/{ThemeName}/`.

**Music** (first match wins; loops; ducks when a video preview plays):

```text
bgm.mp3   bgm.ogg   bgm.wav
music.mp3 music.ogg music.wav
```

You can also put those files in an `audio/` subfolder. Toggle and volume: Settings → Appearance → **Theme Music** / **Theme Music Volume**. Disabled in Applet Mode.

**UI sounds** (WAV only; missing files fall back to the bundled romfs clips):

```text
sfx/nav.wav
sfx/confirm.wav
sfx/toggle.wav
sfx/scrape_complete.wav
```

`nav.wav` / `confirm.wav` / `toggle.wav` may also sit in the theme root. Settings → **Navigation sounds** still controls volume for those.

**List-style system art:** `{systemId}-list.jpg` (also `.png` / `.webp`) in the theme folder, `backgrounds/`, or `images/` — e.g. `snes-list.jpg`. Until those exist, the list preview uses the bundled system placeholder.

**System descriptions** (list preview text): edit

`sdmc:/switch/NXStation/settings/system_descriptions.json`

Keys are system ids (`snes`, `nes`, `favorites`, …). A bundled copy ships in the NRO and is copied there on first launch if the file is missing. Repo template: [`resources/system_descriptions.json`](resources/system_descriptions.json).

Developers building locally (required for scraping and Cloud Save in custom builds):

1. Copy `include/app/ScreenScraperCredentials.local.hpp.example` → `ScreenScraperCredentials.local.hpp` and add your ScreenScraper **developer API** registration (devid/devpassword).
2. Copy `include/app/GoogleOAuthDefaults.local.hpp.example` → `GoogleOAuthDefaults.local.hpp` and add your Google OAuth **Desktop app** client id/secret (Cloud Save / Drive).

Both `*.local.hpp` files are gitignored — never commit real keys to the public repo. Official release NROs are built with these files present on the maintainer machine only.

Regenerate the website blog after publishing a GitHub release:

```bash
python website/scripts/generate_blog.py
```

Keep root `version.json` in sync with the release tag (used by the in-app updater when `api.github.com` is unreachable).

## Launch flow

1. User selects a game in the list → **A** launches immediately (metadata is optional via **Y**)
2. Frontend validates ROM + core paths
3. `AppState::beginHandoff()` flushes video, textures, worker queue
4. `envSetNextLoad(core.nro, "core.nro \"rom path\"")` — core-direct (Switch standard)
5. Borealis quits on the next frame so RetroArch boots with free RAM

## Desktop stub

```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DSF_ENABLE_FFMPEG=OFF
cmake --build build_desktop -j
```

Useful for UI iteration; NRO handoff is logged only.

## Notes

- Prefer **Title Override** over Applet Mode for video previews and PDF manuals.
- Scraper rate-limit delay defaults to 350 ms (`settings.request_delay_ms`).
- Hover delay for video is 1–6 seconds (Settings → **Video Preview Delay**; default 2 s).
- Theme music / SFX: see **Theme music and sound effects** above.
