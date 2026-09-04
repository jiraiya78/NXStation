# UI Showcase carousel assets

Drop media into this folder. The carousel supports **GIF**, **MP4/WebM video**, and static **JPG/PNG**.

## Recommended specs

| Format | Use for | Notes |
|--------|---------|--------|
| **MP4** + **WebM** | Video previews, carousel motion | Preferred — smaller and sharper than GIF. Muted, looped, 16:9, ~1280×720. |
| **GIF** | Simple UI loops, scraper progress | Works in `<img>`; paused when slide is off-screen. |
| **JPG/PNG** | Static screenshots | No animation. |
| **SVG** | Posters / placeholders | Used as `poster` on videos until clip loads. |

## Expected filenames (match `index.html`)

| Slide | Video (optional) | GIF fallback | Static fallback |
|-------|------------------|--------------|-----------------|
| Main carousel | `carousel.mp4`, `carousel.webm` | `carousel.gif` | `carousel-view.svg` (poster) |
| Scraper | — | `scraper.gif` | — |
| Game list + preview | `game-list.mp4`, `game-list.webm` | `game-list.gif` | `list-view.svg` (poster) |
| Themes | — | `themes.gif` | — |
| ES-DE migrate | — | — | `easy-migrate.jpg` |
| Settings | `settings.mp4`, `settings.webm` | `settings.gif` | `settings.jpg` (poster + static fallback) |

If a video file is missing, the carousel shows the paired `.gif`. If the GIF is also missing, slides with `data-fallbacks` continue to `.jpg` (settings). If all media is missing, the video `poster` or matching `.svg` placeholder remains visible.

## Tips

- Record Switch footage at 1280×720, export H.264 MP4 (and optional WebM for smaller size).
- For GIFs, keep loops short (3–8 s) and under ~5 MB when possible.
- Only the **active** slide plays animation; others are paused to save CPU/battery.
