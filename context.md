# NXStation — Agent Context

Nintendo Switch homebrew ROM frontend (repo: `NXStation`). Borealis UI (xfangfang fork), RetroArch core-direct launch, ScreenScraper metadata, optional FFmpeg video previews.

**Current version:** 1.0.0 (2026-08-28). See [`changelog.md`](changelog.md) and [`release_v1.0.0.md`](release_v1.0.0.md).

## What it does

- Scans ROMs from `sdmc:/roms/{systemId}/` per `roms_config.json`
- Full-width system library (no sidebar `TabFrame`); Settings pushed via **+**
- Launches games via **core** `.nro` chain-load (`envSetNextLoad`)
- Manual scrape only (**Y** in game list → scrape menu); no auto-scrape on browse
- **A on game launches immediately** (no game detail screen)
- Favorites (**X**) persisted to `user_favorites.json`, sorted to top
- Last Played virtual system: most recent first; rows show system acronym + date/time
- In-app updater (Settings → **Check for Updates**) via GitHub Releases API
- System carousel with configurable transitions (Fade / Slide / Crossfade / **Zoom** default / None); optional **list-style system browser** (game-list layout + per-system art/descriptions)
- Theme **background music** (`bgm.mp3` in theme folder) and optional per-theme **SFX**; bundled sample BGM by Vlad Krotov (Pixabay)
- ~1 s zoom+fade launch transition before RetroArch
- Y menu → **Personality Metrics** / **Playtime Analytics** (see [Analytics & metrics](#analytics--metrics))
- PDF **game manual** viewer (MuPDF when `SF_ENABLE_MUPDF=ON`): scrape **Manual** toggle (off by default); read via **Y → Menu → Show Metadata → Manual**
- Local box art / video discovery from standard data paths (no JSON required for media alone)
- Restores game list focus after returning from RetroArch (`navigation_state.json`)

## Critical paths (SD card)

| Path | Purpose |
|------|---------|
| `sdmc:/switch/NXStation/NXStation.nro` | Homebrew binary — deploy here (also updater target) |
| `sdmc:/switch/NXStation/cores/` | Optional standalone core `.nro` files (e.g. `tico-dolphin.nro`, `tico-azahar.nro`) — set per system in Settings → **Core Paths** |
| `sdmc:/switch/NXStation/settings/roms_config.json` | Systems, cores, default scraper keys |
| `sdmc:/switch/NXStation/settings/user_cores.json` | Per-system core path overrides (Settings UI) |
| `sdmc:/switch/NXStation/settings/user_screenscraper.json` | Scraper website login override (`ssid`, `sspassword`) |
| `sdmc:/switch/NXStation/settings/user_favorites.json` | Favorite ROM paths per system |
| `sdmc:/switch/NXStation/settings/user_last_played.json` | Last Played history (`systemId`, `romPath`, `playedAt` unix seconds) |
| `sdmc:/switch/NXStation/settings/navigation_state.json` | Restore system + game index after RA handoff |
| `sdmc:/switch/NXStation/log/boot.log` | Startup milestones |
| `sdmc:/switch/NXStation/data/meta/{systemId}/{romStem}.json` | Cached metadata per game |
| `sdmc:/switch/NXStation/data/artwork/{systemId}/` | Box art / logos |
| `sdmc:/switch/NXStation/data/video/{systemId}/` | Video previews |
| `sdmc:/switch/retroarch_switch.nro` | RetroArch frontend (menu only) |
| `sdmc:/retroarch/cores/*_libretro_libnx.nro` | Libretro cores |
| `sdmc:/roms/{systemId}/` | ROM files |
| `sdmc:/roms/{systemId}/manuals/{stem}-manual.pdf` | Scraped or hand-copied game manual PDF |
| `sdmc:/switch/NXStation/data/playtime_pending.json` | In-flight NXStation play session (written at launch, removed on commit) |
| `sdmc:/switch/NXStation/data/playtime_nxstation.json` | NXStation playtime + launch-count aggregates per ROM |
| `sdmc:/retroarch/playlists/*.lpl` | RetroArch playlists (runtime strings, last played) |
| `sdmc:/retroarch/playlists/logs/**/*.lrtl` | RetroArch per-title runtime logs (when enabled) |

**Build output** (local): `build_switch/NXStation.nro` — copy to SD.

**Romfs fallback**: `romfs:/roms_config.json` if SD config missing.

## Manual media files (testing without scraper)

Matched by **ROM filename stem** (no extension). Example ROM: `Chrono Trigger.sfc` → stem `Chrono Trigger`.

| Type | Path pattern | Formats |
|------|--------------|---------|
| Box art | `data/artwork/{systemId}/{stem}_box.{ext}` | `.png`, `.jpg`, `.jpeg`, `.webp` |
| Logo | `data/artwork/{systemId}/{stem}_logo.{ext}` | same |
| Video | `data/video/{systemId}/{stem}.{ext}` | `.mp4` (best), `.webm`, `.mkv` |

Optional metadata JSON: `data/meta/{systemId}/{stem}.json` with `description`, `boxArtPath`, `videoPath`, etc.

After adding files: reopen game list or restart app (metadata cache invalidated on rescan in Settings).

`MetadataCache::applyLocalMedia()` probes standard paths on every load/scan. `scanSystem()` calls `invalidateSystem()` first so new files are picked up.

**Video preview**: decoded scaled to max 480×320 before GPU upload (prevents OOM crash). H.264 MP4 recommended. Logs under `[Video]` in `NXStation.log`.

`source/media/VideoPlayer.cpp` runs **two threads per preview**:

| Thread | Work | May sleep? |
|--------|------|------------|
| Demux/video (prio UI+1) | `av_read_frame`, video decode, swscale, PTS pacing | Yes — pacing only |
| Audio decode (prio UI) | audio decode, `swr_convert`, `pushPcm` | Yes — backlog throttle only |

They must stay split. Pacing video on the shared thread starves the audio device (crackle); throttling audio there stops video packets being read (stutter). Audio packets are handed over with `av_packet_move_ref` into a bounded queue; loop/seek uses `requestFlush()` so the codec is flushed on the thread that owns it.

Frames more than 1.5 frame intervals behind the presentation clock are dropped **before** swscale (max 4 in a row), so slow files skip rather than slide out of sync. Pixel buffers are recycled via `recycleFrame()` instead of reallocating ~545 KB per frame.

## Audio

`source/media/SwitchAudout.cpp` owns the single `audout` session shared by UI sfx and video preview audio.

- A **dedicated audio thread** (priority one step above UI) blocks on `audoutWaitPlayFinish` and refills buffers. Never drive it from the render loop — a slow frame would starve it and cause crackle.
- Buffer ownership is tracked per slot (`Slot::queued`) and cleared **only** when libnx hands that exact pointer back. Force-freeing a buffer that the driver still owns re-appends it and corrupts output for the rest of the session.
- Chunks are 1024 frames / 4096 bytes so `buffer_size` keeps the 0x1000 alignment audout requires.
- Preview audio is **primed** (~85 ms buffered) before it starts playing, and stream start/stop/underrun ramp over ~5 ms with a decaying tail, so no step discontinuities.
- The decoder does **not** pace audio by wall clock; it decodes ahead until `PreviewAudio::queuedFrames()` reaches ~256 ms. The audio device paces playback.
- `pushStream()` **rejects** PCM that would overflow the queue rather than evicting what is already buffered — dropping queued samples is audible as a stutter. The decode-side throttle keeps it from ever filling.

## ScreenScraper

Website login in `roms_config.json` → `screenscraper` block (`ssid`, `sspassword`, `softname`); user overrides in `settings/user_screenscraper.json`.

**Developer API credentials** (`devid` / `devpassword`) are compiled from `include/app/ScreenScraperCredentials.local.hpp` (copy from `.example`; gitignored). Do not put dev keys in JSON on the SD card.

| Field | Location |
|-------|----------|
| `devid` / `devpassword` | `include/app/ScreenScraperCredentials.local.hpp` (gitignored) |
| `ssid` / `sspassword` | `roms_config.json` or `settings/user_screenscraper.json` |
| `softname` | `roms_config.json` |

Scraped images are saved as JPEG (`{stem}-image.jpg`, `{stem}-thumb.jpg`) under each system's `images/` folder. Videos prefer `video-normalized` (smaller) when available.

Scrape menu (**Y**) lets you toggle **Box Art**, **Thumbnail**, **Video**, **Manual**, and **Optimized Media** before running a batch.

**Manual** (off by default): downloads ScreenScraper manual as PDF to `roms/{systemId}/manuals/{stem}-manual.pdf`. View in-app via `ManualViewerView` (cover+spread or single-page layout, pinch/LT/RT zoom, stick pan). Requires MuPDF at build time (`SF_ENABLE_MUPDF`, `scripts/build_mupdf_switch.sh`).

**Optimized Media** picks the smallest media variant the API reports, appends `maxwidth`/`outputformat=jpg` to ScreenScraper media URLs so the server resizes before sending, and re-encodes locally via `source/scraper/ImageOptimizer.cpp`. Media entries usually have **no size field**, so selection must always fall back to the first matching entry — comparing an unknown size against `SIZE_MAX` silently discards every candidate.

**Settings → Appearance → Game Art** chooses box art vs thumbnail in the game list preview.

**HTTP 403** = API rejecting requests (bad/unapproved dev credentials). Scrape UI/network path can still be tested; scraping won't succeed until dev ID is approved.

ScreenScraper `systemeid` per system is set in `roms_config.json` as `ssSystemId`. Reference list: https://www.screenscraper.fr/index.php?action=systemesListe

## Launch model

**Game launch** (core-direct):

```text
target = sdmc:/retroarch/cores/foo_libretro_libnx.nro
args   = sdmc:/retroarch/cores/foo_libretro_libnx.nro "sdmc:/roms/.../game.ext"
```

Do **not** use `-L` via `retroarch_switch.nro` for game launch.

**Standalone cores** (Tico Dolphin, Tico Azahar, etc.): Settings → **Core Paths** can point at any `.nro` under `sdmc:/switch/NXStation/cores/`. NXStation chain-loads the core with the ROM path; exit behavior and stability depend on the core — experimental integration.

**RetroArch menu** (Settings → Open RetroArch Menu):

```text
target = sdmc:/switch/retroarch_switch.nro
args   = --menu
```

- `beginHandoff()` before quit; `NavigationState::persistForHandoff()` saves focus
- Quit deferred via `brls::sync()` — do **not** call `appletUnlockExit()` manually
- After RA closes, user lands in Homebrew Menu (Switch limitation); reopen NXStation to restore list

## Analytics & metrics

**Where:** main carousel **Y** menu → **Personality Metrics** or **Playtime Analytics** (`MainMenuOptionsView` → `PlaytimeScreens.cpp` → `AnalyticsReportView`).

**Data loader:** `loadRetroArchPlayLogs()` in `source/analytics/RetroArchPlaytime.cpp` merges three sources into one `GamePlayLog` list per ROM:

| Source | Path | Fields used |
|--------|------|-------------|
| RetroArch playlists | `sdmc:/retroarch/playlists/*.lpl` (+ legacy `content_history.lpl` / `content_runtime.lpl`) | `runtime`, last played |
| RetroArch runtime logs | `sdmc:/retroarch/playlists/logs/**/*.lrtl` | Per-title seconds + timestamp (requires RA **Save runtime log**) |
| NXStation sessions | `sdmc:/switch/NXStation/data/playtime_nxstation.json` | `playtimeSeconds`, `launchCount`, `lastPlayedUnix` |

NXStation time is **added** to RetroArch time for the same ROM path. Metadata enrichment (genre, release year, system name) comes from NXStation `data/meta/` when available.

### NXStation session tracking

For **core-direct** launches (`NroLauncher::launch`), RetroArch may not write `.lrtl` entries. NXStation tracks these itself:

1. **`beginSession()`** on launch — commits any previous pending session first (quick game-switch safe), increments `launchCount` for the new title, writes `data/playtime_pending.json` (`systemId`, `romPath`, `romName`, `coreName`, `startUnix`).
2. **`commitPendingSession()`** on return to NXStation (`main.cpp` after handoff) — computes duration; sessions **< 5 s** are discarded; otherwise playtime is merged into `playtime_nxstation.json` under a normalized ROM-path key.

**Not tracked:** standalone cores (Tico Dolphin/Azahar, etc.), games launched only from the RetroArch menu without NXStation, unless RetroArch runtime logging is enabled separately.

### Personality Metrics

`calculatePersonalityMetrics()` — gamer “tag” from playtime distribution:

- Tags include *16-Bit Purist*, *8-Bit Pioneer*, *Arcade Junkie*, *Polygon Crusader*, *JRPG Scholar*, *Serial Sampler*, *Laser-Focused Completionist*, *Retro Renaissance Gamer*
- Nostalgia breakdown by release decade (70s–2000s+)
- “Time-warp” fun stats (Mario clears, flight hours, arcade quarters)
- Icons from `romfs:/img/metrics/` via `MetricIcons.cpp`

### Playtime Analytics

`analyzePlaytimeHabitsFromLogs()` builds **synthetic sessions** (one per title: total playtime + last-played timestamp — not true per-session history from RetroArch).

Reports include:

- **Session style** — average session length, gaming-style label (Micro-Burst / Casual / Deep Dive / Marathon)
- **Time of day** — morning / afternoon / evening / night-owl percentages
- **Activity heatmap** — up to 365 days with play, intensity 0–4
- **Backlog dust** — games with < 10 min playtime, last touched 30+ days ago
- **Rankings** (`buildPlayRankings()`, top 15) — top games and systems by **playtime** and by **launch count** (launch counts from NXStation opens only)

### Key analytics files

| File | Role |
|------|------|
| `source/analytics/RetroArchPlaytime.cpp` | Load + merge RA + NXStation logs |
| `source/analytics/PlaySessionTracker.cpp` | Pending session + aggregates |
| `source/analytics/PersonalityMetrics.cpp` | Gamer tags / decade breakdown |
| `source/analytics/PlaytimeHabits.cpp` | Habits, heatmap, backlog dust |
| `source/analytics/PlayRankings.cpp` | Top games/systems rankings |
| `source/ui/PlaytimeScreens.cpp` | Report UI assembly |
| `include/analytics/PlaytimeTypes.hpp` | Shared structs |

**RetroArch setting:** Settings → Saving → **Save runtime log** (or equivalent) must be on for `.lrtl` data; without it, NXStation-launched games still accrue time via session tracking, but RA-only play may be missing.

**Tests:** `tests/playtime_analytics_test.cpp` (host build) covers personality tags and habits math.

## UI structure

```text
MainActivity → SystemsTab (carousel — full-screen system backdrop, loops ← →)
  + → PushedActivity(SettingsTab hub → section sub-screens)
  Y → MainMenuOptionsView (search, jump, scan, cloud sync, analytics, settings)
  A → PushedActivity(GameListView)
    A → launch game (core-direct)
    Y → GameOptionsMenuView (brls::Dialog popup)
          → GameDetailView (metadata only) → ManualViewerView (PDF manual)
          → ScrapeMenuView (brls::Dialog popup) → ScrapeProgressView
    X → toggle favorite
```

**Popup menus**: `GameOptionsMenuView::present()` / `ScrapeMenuView::present()` wrap the
menu box in a `brls::Dialog` and call `open()`. Dialogs are translucent activities, so the
game list keeps rendering (and the video preview keeps playing) underneath. Selection
closes via `dialog->close(cb)` so the callback runs after the pop.

Theme: `source/ui/Theme.cpp` — Fluent-inspired dark. Font: `romfs:/font/font.ttf` (Nunito if build fetch succeeds; else borealis fallback). Drop `Nunito-Regular.ttf` into `resources/font/font.ttf` for rounded font.

Navigation SFX: `source/ui/UiSfx.cpp` — soft Borealis sounds at lower pitch.

Box art placeholders: `romfs:/img/systems/{systemId}.png` (generated at build by `scripts/generate_system_placeholders.py`).

## Navigation (gamepad)

| Button | Main menu (carousel) | Game list | Pushed screens |
|--------|----------------------|-----------|----------------|
| **A** | Open system | **Launch game** | Confirm |
| **B** | — | — | Back (pop activity) |
| **+** (START) | Settings | — | — |
| **← → / L / R** | Prev / next system (loops) | Page scroll | — |
| **X** | — | Toggle favorite | — |
| **Y** | — | Scrape / metadata menu | — |

**− (BACK) does not quit** — use Home button to exit. `setGlobalQuit(false)`; no `registerExitAction` on activities.

List rows: `setLineBottom(0)` on `DetailCell` to hide separators.

## Borealis API notes (xfangfang fork)

| Wrong (old docs) | Correct |
|------------------|---------|
| `brls::ListItem` | `brls::DetailCell` + `#include <borealis/views/cells/cell_detail.hpp>` |
| `onShow()` / `onHide()` | `willAppear()` / `willDisappear()` |
| `Button::setEnabled()` | `Button::setState(ButtonState::ENABLED\|DISABLED)` |
| `TabFrame` sidebar for settings | `SystemsTab` + push `SettingsTab` on **+** |

Settings XML: `resources/xml/tabs/settings.xml` — **not** borealis demo settings. Settings rows build on the **first frame** after push (deferred `rebuild()` in `willAppear`) so the activity opens immediately instead of blocking on ~50 row creation.

Game list XML: `resources/xml/views/game_list.xml` — preview card on right; box art in centered `games/art_frame` (380 px tall, art scales to nearly the card width). The video preview is added to the same frame at runtime and swaps with the box art; there is no separate video box.

Description auto-scrolls: `GameListView::tickDescriptionScroll()` holds 2.5 s at the top, creeps at 28 px/s, holds 3 s at the bottom, then jumps back. Offsets are only written on whole-pixel changes because `ScrollingFrame::setContentOffsetY` calls `invalidate()`.

**Game list row scrolling (D-pad hold)** — implemented in `GameListView::tickAcceleratedNavigation()` / `stepListNav()`:

1. **Consume default D-pad** — register empty actions on `BUTTON_NAV_UP` / `BUTTON_NAV_DOWN` so Borealis does not also move focus.
2. **`pollAcceleratedHold()`** — repeat count (not wall time) drives speed tiers in `repeatIntervalForCount()`:
   - Slow: repeats 0–4 → 240 ms
   - Medium: 5–13 → 100 ms
   - Fast: 14–39 → 32 ms
   - Fast2: 40–99 → 18 ms
   - Fast3: 100+ → 8 ms
3. **Smooth list motion** — `RecyclerFrame::selectRowAt(..., true)` (animated scroll) on every step.
4. **Light preview while repeating** — `applyLightPreview()` updates description text only; box art / video wait until the button is released (`navPreviewDeferred_` flush calls `onGameFocused(..., 0)` with no carousel slide).
5. **Avoid animated page jumps for huge lists** — `jumpToIndex()` uses `selectRowAt(..., false)` for L/R page and alphabet jump.

Do **not** call full `onGameFocused()` on every repeat step — video decode + preview transitions cause jank and dizziness.

**Floating popup menus** — use `makePopupMenuScroller()` from `FocusedMenuDialog.cpp` (clipped host + hidden scrollbar). `registerPopupPageActions()` pages with `setContentOffsetY(..., false)` to avoid bleed during L/R scroll.

**Hidden views never get `frame()`** (`View::frame` early-returns unless `VISIBLE`). A view that must do work before it can become visible — like `VideoPreviewView` waiting for its first decoded frame — needs its parent to drive it; hence `VideoPreviewView::pump()` called from `GameListView::frame()`.

**List rebuild safety**: call `brls::Application::giveFocus(scroller)` before `listBox->clearViews()` to avoid crash on **Y** refresh.

**Lifecycle**: views that async-callback into UI use `std::shared_ptr<bool> alive_`; set false in destructor/`willDisappear`.

**Scrape progress**: defer menu push via `brls::sync()` so **A** doesn't hit Abort; disable Abort button focus while running.

**Cloud restore (Phase 1)**: Settings → **Cloud Restore** lists NXStation `retroarch-saves-*.zip` backups from Google Drive (`NXStation/RetroArch` folder). Restore reads `savefile_directory` / `savestate_directory` from `sdmc:/retroarch/retroarch.cfg` at restore time, creates a local pre-restore ZIP under `data/cloud/`, downloads the backup, then **merge-extracts** (overwrite matching paths only; never deletes local-only files). Progress UI mirrors scrape (`CloudRestoreProgressView` + `cloud_restore_progress.xml`): color-coded per-file log, summary at end, opaque `overlay_bg` fullscreen activity. All keys blocked during restore; hold **B for 3 seconds** to abort (partial merge warning). NXStation backups are **not** interchangeable with RetroArch's own cloud backup format.

## Key source files

| Area | Files |
|------|-------|
| Entry | `source/main.cpp` |
| Config | `source/app/Config.cpp`, `include/app/Config.hpp`, `include/app/ScreenScraperCredentials.hpp`, `roms_config.json` |
| Library scan | `source/app/AppState.cpp` |
| Game list | `source/ui/GameListView.cpp`, `resources/xml/views/game_list.xml` |
| Systems menu | `source/ui/SystemsTab.cpp` |
| Settings | `source/ui/SettingsTab.cpp`, `SettingsSectionView.cpp`, `SettingsSections.cpp` |
| Analytics | `source/analytics/RetroArchPlaytime.cpp`, `PlaySessionTracker.cpp`, `PersonalityMetrics.cpp`, `PlaytimeHabits.cpp`, `PlayRankings.cpp`, `source/ui/PlaytimeScreens.cpp` |
| Scrape | `source/scraper/ScraperService.cpp`, `source/ui/ScrapeMenuView.cpp`, `ScrapeProgressView.cpp` |
| Launch | `source/launcher/NroLauncher.cpp` |
| Nav restore | `source/util/NavigationState.cpp` |
| Last Played | `source/util/LastPlayed.cpp` (`playedAt` wall clock; `formatPlayedAt`) |
| App updater | `source/util/AppUpdater.cpp` (GitHub `releases/latest` → download `NXStation.nro`) |
| HTTP | `source/scraper/HttpClient.cpp` (libcurl; timeouts + optional headers) |
| Metadata | `source/scraper/MetadataCache.cpp` |
| Video | `source/media/VideoPlayer.cpp`, `source/ui/VideoPreviewView.cpp` |
| Manual PDF | `source/media/PdfManual.cpp`, `source/media/ManualPages.cpp`, `source/ui/ManualViewerView.cpp` |
| Crash dump | `source/util/CrashHandler.cpp`, `scripts/symbolize_crash.py` |
| Network | `source/util/Network.cpp` (NIFM before curl on Switch) |
| Theme | `source/ui/Theme.cpp` |
| Cloud sync / restore | `source/cloud/CloudSaveService.cpp`, `CloudRestorePickView.cpp`, `CloudRestoreProgressView.cpp` |

## Logging

- Main log: `sdmc:/switch/NXStation/log/NXStation.log`
- Cloud backup/restore log: `sdmc:/switch/NXStation/log/cloud.log` (upload, restore, per-file merge results)
- Scrape log: `sdmc:/switch/NXStation/log/scrape.log`
- At startup, `Logger::trimLogIfNeeded()` keeps the file ≤ 512 KiB (rewrites only when oversized; otherwise one `stat`).

## RetroArch launch / salamander.cfg

Before core-direct launch, NXStation writes:

`sdmc:/retroarch/retroarch-salamander.cfg` → `libretro_path = "<core being launched>"`

RetroArch Switch otherwise keeps a stale salamander core path and can show the wrong boot banner (e.g. Dolphin) even when `argv[0]` is the correct core. Zip/7z titles are especially sensitive because Dolphin’s `.info` lists those extensions.

## In-app updater

Settings → **Check for Updates**:

1. `Network::waitForConnection` then GET  
   `https://api.github.com/repos/jiraiya78/NXStation/releases/latest`
2. Compare `tag_name` (strip leading `v`) to `kAppVersion` from generated `Version.hpp`
3. Prefer release asset named exactly **`NXStation.nro`**
4. Download to `NXStation.nro.tmp`, replace `paths::APP_NRO`, notify user to **restart**

Release process: publish a GitHub Release with a semver tag (`v1.0.0`) and attach `NXStation.nro`. Without that asset name, the check reports an update but cannot install.

`Config::load` merges **new** bundled systems into an existing SD `roms_config.json` (does not overwrite user entries), and prunes retired IDs (currently `ps2`) from SD `roms_config.json` / `user_cores.json`.

**Scan Games / Rescan** calls `Config::reload()` so edits to `settings/roms_config.json` appear without restarting the app. With **Hide Empty Systems** (default **On**), a newly added system shows up after it has at least one scanned ROM.

## Video preview

- Requires **Title Override** (not Applet Mode); `AppState::videoAllowed()` gates enable
- ~1 s hover delay (`Config::hoverDelaySeconds`, default 1.0)
- Plays **in place of the box art** inside `games/art_frame`; `GameListView` swaps
  `boxArt` / `VideoPreviewView` visibility

### `sdmc:` paths must be passed to FFmpeg as `file:sdmc:/...`

FFmpeg resolves everything before the first `:` as a URL scheme (`url_find_protocol`), so
`sdmc:/roms/...` looks like an unknown protocol `sdmc` and never reaches the file handler —
`avformat_open_input` returns `AVERROR_PROTOCOL_NOT_FOUND` (-1330794744). `VideoPlayer`
prefixes paths with `file:`, which FFmpeg strips before calling `open()`, letting the
devkitPro devoptab resolve the mount. Same applies to any future `romfs:` use.

### Decode thread

- Raw libnx `threadCreate`: 2 MiB stack, priority = caller priority + 1 (below the UI thread)
- `threadWaitForExit` **must** be paired with `threadClose` or the stack and handle leak
- Body is wrapped in try/catch — an escaping exception would `std::terminate` with no log
- No `thread_local` in the decode path

### Reading the log

Expected `[Video]` sequence for a working preview:

```text
Selection: path=... exists=yes enabled=yes
Hover delay elapsed — starting decode: ...
Decode thread started (prio=0x2D stack=2048KiB)   <- main thread, after threadStart
Decode thread entered (gen=N)                     <- decode thread is alive
Opening: file:sdmc:/...
Container open: mov,mp4,m4a,3gp,3g2,mj2
Stream info ready (N streams)
Decoder: h264 (640x480)
Decoding started: ... (640x480 -> 426x320)
First frame decoded (426x320)
Frame uploaded to GPU (426x320)
```

Where it stops tells you which layer failed. FFmpeg's own warnings/errors are bridged into
the log under `[FFmpeg]`.

## Crash reports

`source/util/CrashHandler.cpp` overrides libnx's weak `__libnx_exception_handler` /
`__nx_exception_stack` and appends a register dump plus frame-pointer backtrace to
`sdmc:/switch/NXStation/crash.log`. Addresses are printed twice: raw, and as `elf 0x...`
relative to the module base.

```powershell
python scripts/symbolize_crash.py log/crash.log
```

User exception handlers only fire when the host process allows them. If `crash.log` stays
empty, use Atmosphère's report in `sdmc:/atmosphere/crash_reports/` and pass its module base:

```powershell
python scripts/symbolize_crash.py --module-base 0x<base> log/report.log
```

Symbols must come from the **same build** that crashed (`build_switch/NXStation.elf`).

## Build

```powershell
function dmake { docker run --rm -v ${PWD}:/src --workdir /src devkitpro/devkita64 make $args }
dmake          # → build_switch/NXStation.nro
dmake clean
```

PRE_BUILD: copies `roms_config.json` to resources; runs `scripts/generate_icon.py` and `scripts/generate_system_placeholders.py`.

CMake: `-DPLATFORM_SWITCH=ON -DSF_ENABLE_FFMPEG=ON -DSF_ENABLE_MUPDF=ON`. Link order: FFmpeg libs then `z` last. MuPDF is built via `scripts/build_mupdf_switch.sh` on first CMake configure when missing.

After adding new `.cpp` files, may need `cmake ..` in `build_switch` (GLOB cache).

Switch build: **no** `STB_IMAGE_IMPLEMENTATION` in `TextureCache.cpp`.

## Known constraints

- Applet mode: video + high-res textures disabled; notify user to use Title Override
- All GL/NVG UI on main thread; workers for curl/hash/decode
- `beginHandoff()` must flush textures/video before `envSetNextLoad`
- Auto-scrape on focus was removed (caused crashes); scrape is manual only
- Borealis `ScrollingFrame` scrollbar overlaps list — pad list `paddingRight` ~28px

## Supported systems (roms_config.json)

Default systems (28): `atari2600`, `atari5200`, `atari7800`, `atarilynx`, `atarijaguar`, `atarist`, `nes`, `snes`, `n64`, `gba`, `gb`, `gbc`, `nds`, `3ds`, `gc`, `wii`, `megadrive`, `mastersystem`, `gamegear`, `saturn`, `dreamcast`, `cps1`, `cps2`, `neogeo`, `arcade`, `psx`, `psp`, `pce`.

Retired / pruned on load: **PS2** (no practical Switch RetroArch core).

**GameCube / Wii / 3DS** are listed in the default config but need standalone **Tico** cores (`tico-dolphin`, `tico-azahar`) placed in `cores/` and assigned in Settings → Core Paths — not bundled with NXStation.

Add new systems in: `roms_config.json` / `resources/roms_config.json` (include `ssSystemId` from ScreenScraper system list), and optionally `scripts/generate_system_placeholders.py`. Scan/Rescan reloads the SD config so custom entries appear without a full restart.

## Website (`website/`)

Static site for [nxstation.com](https://nxstation.com): landing page, legal pages, user **Wiki** (`wiki.html` — setup, controls, scraping, PDF manuals, Tico cores, analytics, themes), and **Blog** (`blog.html` + `blog/*.html` — release notes mirrored from GitHub).

Regenerate blog posts after a new GitHub release:

```powershell
python website/scripts/generate_blog.py
```

Uses `website/data/releases.json` (GitHub API cache) and local `release_v*.md` for drafts not yet published.
