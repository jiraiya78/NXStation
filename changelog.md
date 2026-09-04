# Changelog

All notable changes to NXStation are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-08-28

### Added

- **List-style system browser** — per-theme `browser="list"` layout with system art, descriptions, and game-list-style navigation (Settings → System Browser Style or theme `browser="list"`).
- **Carousel transitions** — **Crossfade** and **Zoom** presets; **Zoom** is the new default.
- **Theme background music** — loop `bgm.mp3` / `music.mp3` (also `.ogg` / `.wav`) from the active theme folder, `audio/`, `music/`, or `sample/bgm.mp3` on SD.
- **Theme SFX** — optional per-theme `sfx/*.wav` for navigation and confirm sounds.
- **System descriptions** — bundled `system_descriptions.json` with blurbs in the list browser preview card.
- **Video preview delay** — Settings → Appearance: configurable **1–6 s** hover delay before video starts.
- **Screensaver defaults** — bundled `screensaver.jpg` (dark) and `screensaver-light.jpg` (light) when a theme has no custom image.
- **Carousel readability** — halo labels and bottom gradient so titles stay legible on bright or dark system wallpapers.
- **System list art** — `{systemId}-list.jpg` (`.png` / `.webp`) per theme or `backgrounds/` / `images/`.

### Changed

- Refreshed **system carousel backgrounds** for all default systems.
- System list uses the same **List Scrollbar** setting as the game list.
- Settings **List Scrollbar** label applies to both ROM and system lists.
- Default **Carousel Transition** is **Zoom** (existing `user_settings.json` values are kept).

### Fixed

- System list **selection highlight** visible and stable after theme changes and returning from the game list.
- System list **row separators** pick up the correct theme color immediately (not only when focused).
- System list **smooth scrolling** and **wrap** navigation (up/down) aligned with game-list feel.
- Theme refresh no longer leaves stale highlight/separator colors on recycled rows.
- Theme BGM path resolution on Switch (`file:` URL, larger decode thread stack).

### Credits

- Sample theme music (`bgm.mp3`) by **Vlad Krotov** from [Pixabay](https://pixabay.com).

## [0.3.0] - 2026-08-21

### Added

- **Google Drive cloud backup** — Settings → Cloud Save: link account, **Backup Saves to Cloud**, **Auto Cloud Save** after returning from a game (RetroArch saves + save states ZIP upload).
- **Cloud Restore** — pick a Drive backup, merge-extract into current RetroArch paths; pre-restore local ZIP; progress log UI; hold **B for 3 seconds** to abort.
- **Cloud logging** — `sdmc:/switch/NXStation/log/cloud.log` for backup/restore operations.
- **Settings hub** — six sub-screens: App & Launcher, Emulator and Systems, Appearance and Control, Cloud Save, Scraper, Help and About.
- **Playtime rankings** — Playtime Analytics reports top games/systems by playtime and by launch count (NXStation session tracking).
- **Launch count tracking** — per-game open count in `data/playtime_nxstation.json` for NXStation-launched titles.

### Changed

- Cloud UI labels: **Sync Now** → **Backup Saves to Cloud**, **Last Sync** → **Last Backup**.
- Settings opens immediately (rows build on first frame); cloud section refreshes **Last Backup** on open.
- Auto cloud backup skips when offline, when no save files exist, or when a backup is already running (no error toast spam).
- Playtime from NXStation core-direct launches merges into analytics even without RetroArch `.lrtl` logs.

### Fixed

- Settings **D-pad navigation** dead on open until Y menu was opened and closed (focus landed on non-selectable header).
- Cloud restore pick list **D-pad** focus; restore progress **right-stick scroll** after completion.
- **Quick game switching** — previous session committed before starting the next; overlapping auto-backups guarded.
- Offline auto cloud save no longer shows a misleading “couldn’t resolve host” notification.

## [0.2.3] - 2026-08-14

### Added

- Settings → **Carousel Transition** (Fade / Slide / None) for the system carousel and preview panel transitions.
- Settings → **Right Stick Scroll** — optional right-stick scrolling for game description (list preview + metadata popup).
- **Accelerated list navigation** — hold D-pad Up/Down or **L/R** to scroll faster (one row or 8-game page at a time, rate ramps up).
- **L/R paging** in Y menu, scrape menu, and Jump to System when content scrolls.
- Manual viewer **layout mode** (`cover_spread` / `single_page`) saved in `user_settings.json` and restored on open.
- Horizontal **slide transitions** between manual spreads; softer **dialog open/close** (scale + fade).

### Changed

- Default **system order** in Settings core list and main carousel (Arcade → … → PC Engine); `roms_config.json` reordered to match.
- Launch transition uses **aspect-fit** box art (no stretch during zoom).
- Right stick is reserved for **description scroll** and **manual zoom** only — no longer moves list focus or metadata buttons.
- Skipped manual entries in scrape log use **neutral gray** (not red).

### Fixed

- **Video preview crash** after long browsing (FFmpeg `swscale` on MP4s with unspecified pixel format; probe/decode first frame, validate format).
- **Individual scrape** sometimes targeted the wrong game (e.g. PC Engine) when favorites/sort changed list order.
- **Dialog close flicker** on Y menu and other popups.
- **Preview panel** stuck on first game after accelerated list scrolling.
- Custom **`scrape_complete.wav`** preserved across builds when placed in `resources/audio/`.

## [0.2.2] - 2026-08-12

### Added

- Y menu → **◆ Game Manual** (top entry when a manual is recorded for the focused game); toast if the PDF file is missing.

### Changed

- Manual viewer controls: **D-pad / L·R** for pages; **right stick up/down** zoom; **left stick** pan when zoomed; stick nav no longer turns pages.

### Fixed

- Manual viewer: first page no longer renders as a blank square on open (defer texture upload until layout is ready; cache invalidates on slot resize).
- Manual viewer: game list selection no longer bleeds through or stays navigable under the fullscreen viewer; deferred overlay/highlight restore on close.

## [0.2.1] - 2026-08-11

### Fixed

- In-app updater: release romfs before replacing the running NRO (fixes `0xE02 TargetLocked`); overwrite via native FS then `envSetNextLoad` the installed path; download progress %; restart confirmation dialog; staged `NXStation_update.nro` fallback; flushed updater logs.

## [0.2.0] - 2026-08-11

### Added

- More default systems in `roms_config.json`: Atari 5200/7800/Lynx/Jaguar/ST, GameCube, Wii, Saturn, Arcade, and 3DS (`tico-azahar`).
- System carousel **fade** when browsing left/right.
- Game launch **zoom-in + fade-out** transition (~1 second) before RetroArch handoff.
- Y menu → **Personality Metrics** (gamer tags, nostalgia breakdown, time-warp stats from RetroArch logs).
- Y menu → **Playtime Analytics** (session style, time-of-day, heatmap, backlog-dust score).
- PDF **game manual** viewer (`ManualViewerView`, MuPDF): scrape **Manual** toggle in scrape menu (off by default); open from **Y → Menu → Show Metadata → Manual**; saves to `roms/{systemId}/manuals/{stem}-manual.pdf`.
- Website **Wiki** (`website/wiki.html`) — setup, controls, scraping, manuals, Tico standalone cores, analytics, themes.
- Website **Blog** (`website/blog.html`) — release notes mirrored from GitHub Releases (`website/scripts/generate_blog.py`).

### Changed

- **Hide Empty Systems** defaults to **On** (existing `user_settings.json` values are kept).
- Editing `settings/roms_config.json` is picked up by **Scan Games / Rescan Libraries** (config reload) — no full app restart required.
- On upgrade, systems present in the bundled config but missing from the SD copy are **merged in** automatically (user entries are preserved).
- `ps2` remains retired/pruned; `3ds` is no longer auto-removed.
- Playtime Analytics and Personality Metrics document that only RetroArch runtime logs are supported (standalone Tico cores excluded).

### Fixed

- In-app updater: stop canceling the NIFM session after each HTTP request (fixes `Couldn't resolve host name` on update check/download); retry transient network errors; fall back to `version.json` when the GitHub API is unreachable.

## [0.1.8] - 2026-08-07

### Added

- **Check for Updates** in Settings: queries the GitHub Releases API (`jiraiya78/NXStation`), compares the latest `tag_name` to the build version, and can download `NXStation.nro` over the installed app (restart required).
- Last Played rows show **date and time** last played (`YYYY-MM-DD HH:MM`) next to the system acronym.

### Changed

- Last Played timestamps use the Switch user/network clock and format with console timezone rules (`timeToCalendarTimeWithMyRule`) so the displayed time matches system Settings.
- Fixed Last Played times all collapsing to the same clock time (e.g. `05:00`): JSON number dump used 6-digit precision (`1.75e+09`), so every recent `playedAt` reloaded as one unix second. Integers now dump in full; `playedAt` is stored as a decimal string.
- Last Played remains ordered **most recent first** (game list no longer re-sorts that section A–Z).
- Before each game launch, rewrite `sdmc:/retroarch/retroarch-salamander.cfg` `libretro_path` to the core being launched so RetroArch’s boot banner / core info match (fixes sticky “Nintendo Wii/GameCube (Dolphin)” overlay, including for zip/7z).
- `NXStation.log` is auto-trimmed at launch when larger than 512 KiB (keeps the last ~256 KiB). Under the limit this is a single `stat` — no per-frame cost.

### Removed

- Nintendo 3DS (`3ds`) from the default `roms_config.json` (no practical RetroArch 3DS core on Switch). Existing SD configs and `user_cores.json` auto-prune the `3ds` entry on load.

## [0.1.7] - 2026-08-05

### Fixed

- After **Jump to Letter**, A / X / Y / L / R work again (letter pick called non-virtual `Dialog::close`, so the overlay block never cleared).
- Metadata popup is middle-centered again (content no longer wider than the dialog frame).
- Metadata popup focus is limited to **Rename** / **Delete**; B closes. D-pad no longer selects the whole panel or restyles its chrome.

## [0.1.6] - 2026-08-05

### Added

- **Jump to System** in the main Y menu.
- Favorites / Last Played rows show a short system acronym again (SNES, MEGADRIVE, PSX, …).
- **Rename ROM** and **Delete ROM** in the game list Y menu and on the metadata popup.
- Metadata popup shows the ROM filename to tell similar/duplicate files apart.

### Changed

- Metadata opens as a slightly inset popup dialog instead of a full screen.
- Scraper status lines no longer repeat the title after `OK`.
- Scraper errors use clearer wording (e.g. `No match (HTTP 404)`, `Server rejected (HTTP 403)`).

### Fixed

- After Jump to Letter / Search from the game list Y menu, A / X / Y / L / R stopped working (stuck overlay block).
- Selection highlight from the game list no longer floats over the metadata popup or system scan screen; video preview stops while those are open.
- Game list behind the metadata popup can no longer be navigated.

## [0.1.5] - 2026-08-05

### Fixed

- **Jump to letter** skips starred games pinned at the top, so it lands in the alphabetical list.
- Game list no longer shows a blank gap under the system name (empty recycler section header was measuring as a full row).
- Fast L/R page scrolling no longer desyncs the selection highlight from the art/preview (and A now launches the previewed game consistently). Instant jumps also stop the selection rectangle from briefly flashing behind the system title when holding R.

## [0.1.4] - 2026-08-05

### Added

- **Custom ROMs Path** in Settings: browse for a different ROM root folder (e.g. shared with Tico) instead of the default `sdmc:/roms`; triggers an automatic rescan.
- **Folders & Logs** row in Settings compiles every data/settings/log path into a single scrollable screen instead of a long list of separate rows.
- Left analog stick now browses the system carousel (hold to auto-repeat), in addition to D-pad and L/R.

### Changed

- Library scan progress (`Processing X/Y games`) now updates smoothly at the display refresh rate instead of jumping straight to the final count — the scan no longer floods the UI thread with per-item updates.
- Game list is now a virtualized/recycled list: opening a 900-game system only builds the ~15 rows actually on screen instead of all of them, matching the cost of opening a 20-game system. Fixes the severe input lag (delayed A press, dropped quick A→B) on large libraries.
- Settings **About** version now always reflects the actual build version instead of a hardcoded string.

## [0.1.3] - 2026-08-05

### Fixed

- Game list focus highlight no longer shows behind the Y menu on large libraries (overlay blocking + hide highlight on scroller).
- Large lists (600+ games) build in fewer chunks to reduce stutter; Y menu is blocked until the list finishes building.
- **Scrape Missing Art** now skips games that already have art on SD (checks ES-DE image folders, not only the `scraped` flag).
- Scrape metadata and library cache persist after each successful game so aborted batches don't cause re-scrapes on the next run.
- Game list refreshes from AppState after scrape so the next batch uses up-to-date metadata.

## [0.1.2] - 2026-08-05

### Added

- Library index cache on SD (`data/cache/library_index.json`) so the ROM list survives NRO handoff relaunches (e.g. returning from RetroArch).

### Changed

- Scrape completion sound updated to a Double Dragon (NES) stage-clear style jingle.
- Game list loads a system on demand if the in-memory index is missing but the library was previously scanned.
- Startup reloads the library from cache when available; falls back to a one-time rescan only if the cache is missing.

### Fixed

- Game list appearing empty after returning from RetroArch until browsing away and back.

## [0.1.1] - 2026-08-04

### Added

- Completion sound when a scrape batch finishes successfully (chiptune-style fanfare).
- **Scan Games** in the Y menu on the main carousel (full library) and in the game list (current system only).
- First-launch library scan with a centered progress screen (system name + `Processing X/Y games`).
- `changelog.md` for release notes.

### Changed

- Library scan runs once on first start; further updates are manual via **Scan Games** or Settings → Rescan Libraries.
- Scraper log no longer auto-scrolls after a batch finishes so you can review entries before pressing B.
- Screensaver video has top padding so previews are not flush with the screen edge.
- Switch stays awake during scraping (screen may dim; auto-sleep disabled until scrape ends).

### Removed

- PlayStation 2 (`ps2`) from the default `roms_config.json` (no practical PS2 core on Switch).

## [0.1.0] - 2026-08-03

### Added

- Initial public release: ES-DE-style ROM browser, ScreenScraper integration, RetroArch launch, themes, screensaver, forwarder install, Title Override enforcement.
