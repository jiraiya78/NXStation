#!/usr/bin/env python3
"""Generate blog HTML from GitHub releases and local release notes."""

from __future__ import annotations

import html
import json
import re
import urllib.request
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLOG_DIR = ROOT / "blog"
DATA_FILE = ROOT / "data" / "releases.json"
REPO = "jiraiya78/NXStation"
API_URL = f"https://api.github.com/repos/{REPO}/releases?per_page=100"


def fetch_releases() -> list[dict]:
    if DATA_FILE.is_file():
        return json.loads(DATA_FILE.read_text(encoding="utf-8"))
    req = urllib.request.Request(API_URL, headers={"User-Agent": "NXStation-blog-generator"})
    import ssl

    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(req, timeout=30, context=ctx) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError:
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        with urllib.request.urlopen(req, timeout=30, context=ctx) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    DATA_FILE.parent.mkdir(parents=True, exist_ok=True)
    DATA_FILE.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return data


def extract_local_release(tag: str) -> dict | None:
    path = ROOT.parent / f"release_{tag}.md"
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8")
    title_match = re.search(r"## Proposed release title\s+```\s*\n(.+?)\n```", text, re.S)
    body_match = re.search(r"## Release description \(markdown\).*?```markdown\s*\n(.+?)\n```", text, re.S)
    if not title_match or not body_match:
        return None
    return {
        "tag_name": tag,
        "name": title_match.group(1).strip(),
        "body": body_match.group(1).strip(),
        "html_url": f"https://github.com/{REPO}/releases/tag/{tag}",
        "published_at": None,
        "assets": [],
        "draft_local": True,
    }


def parse_changelog_date(tag: str) -> str | None:
    changelog = ROOT.parent / "changelog.md"
    if not changelog.is_file():
        return None
    m = re.search(rf"## \[{re.escape(tag.lstrip('v'))}\] - (\d{{4}}-\d{{2}}-\d{{2}})", changelog.read_text(encoding="utf-8"))
    return m.group(1) + "T12:00:00Z" if m else None


def inline_md(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r"`([^`]+)`", r"<code>\1</code>", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2" target="_blank" rel="noopener noreferrer">\1</a>', text)
    return text


def markdown_to_html(md: str) -> str:
    lines = md.replace("\r\n", "\n").split("\n")
    out: list[str] = []
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        if stripped == "---":
            out.append("<hr />")
            i += 1
            continue

        if stripped.startswith("### "):
            out.append(f"<h3>{inline_md(stripped[4:])}</h3>")
            i += 1
            continue

        if stripped.startswith("#### "):
            out.append(f"<h4>{inline_md(stripped[5:])}</h4>")
            i += 1
            continue

        if re.match(r"^[-*] ", stripped):
            out.append("<ul>")
            while i < len(lines) and re.match(r"^[-*] ", lines[i].strip()):
                out.append(f"<li>{inline_md(lines[i].strip()[2:])}</li>")
                i += 1
            out.append("</ul>")
            continue

        if re.match(r"^\d+\.\s", stripped):
            out.append("<ol>")
            while i < len(lines) and re.match(r"^\d+\.\s", lines[i].strip()):
                item = re.sub(r"^\d+\.\s*", "", lines[i].strip())
                out.append(f"<li>{inline_md(item)}</li>")
                i += 1
            out.append("</ol>")
            continue

        para_lines = [stripped]
        i += 1
        while i < len(lines):
            nxt = lines[i].strip()
            if not nxt or nxt == "---" or nxt.startswith("#") or re.match(r"^[-*] ", nxt) or re.match(r"^\d+\.\s", nxt):
                break
            para_lines.append(nxt)
            i += 1
        out.append(f"<p>{inline_md(' '.join(para_lines))}</p>")

    return "\n            ".join(out)


def format_date(iso: str | None) -> str:
    if not iso:
        return "Unreleased"
    try:
        dt = datetime.fromisoformat(iso.replace("Z", "+00:00"))
        return dt.strftime("%B %d, %Y")
    except ValueError:
        return iso[:10]


def excerpt_from_body(body: str, limit: int = 200) -> str:
    for line in body.splitlines():
        line = line.strip()
        if not line or line == "---" or line.startswith("#"):
            continue
        plain = re.sub(r"[*`\[\]]", "", line)
        plain = re.sub(r"\([^)]*\)", "", plain)
        if len(plain) > limit:
            return plain[: limit - 1].rstrip() + "…"
        return plain
    return "Release notes for NXStation."


def nro_download_url(release: dict) -> str | None:
    for asset in release.get("assets", []):
        if asset.get("name") == "NXStation.nro":
            return asset.get("browser_download_url")
    if release.get("draft_local") and not release.get("published_at"):
        return None
    tag = release.get("tag_name", "")
    if tag and release.get("published_at"):
        return f"https://github.com/{REPO}/releases/download/{tag}/NXStation.nro"
    return None


def page_shell(*, title: str, description: str, depth: int, active: str, main: str) -> str:
    prefix = "../" * depth
    blog_active = ' aria-current="page" class="nav__link--active"' if active == "blog" else ""
    wiki_active = ' aria-current="page" class="nav__link--active"' if active == "wiki" else ""
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <script>
    (function () {{
      var k = "nx-theme", s = localStorage.getItem(k);
      var t = s === "light" || s === "dark" ? s
        : (window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark");
      document.documentElement.setAttribute("data-theme", t);
      var m = document.querySelector('meta[name="theme-color"]');
      if (m) m.setAttribute("content", t === "light" ? "#FAF5FF" : "#0C0612");
    }})();
  </script>
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <meta name="description" content="{html.escape(description)}" />
  <meta name="theme-color" content="#0C0612" />
  <title>{html.escape(title)}</title>
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
  <link href="https://fonts.googleapis.com/css2?family=Nunito:wght@400;600;700&family=Silkscreen:wght@400;700&family=Space+Grotesk:wght@500;600;700&display=swap" rel="stylesheet" />
  <link rel="stylesheet" href="{prefix}styles.css" />
  <link rel="icon" href="{prefix}assets/icons/favicon.svg" type="image/svg+xml" />
</head>
<body>
  <div class="crt-overlay" aria-hidden="true"></div>

  <header class="site-header" id="top">
    <nav class="nav glass" aria-label="Primary">
      <div class="nav__products">
        <a class="nav__brand is-active" href="{prefix}index.html">
          <img src="{prefix}assets/icons/logo.svg" alt="" width="28" height="28" />
          <span>NXStation</span>
        </a>
        <a class="nav__brand nav__brand--b" href="{prefix}nxstationb/index.html">NXstationB</a>
      </div>
      <ul class="nav__links" id="nav-menu">
        <li><a href="{prefix}index.html#features">Features</a></li>
        <li><a href="{prefix}index.html#setup">Setup</a></li>
        <li><a href="{prefix}index.html#faq">FAQ</a></li>
        <li><a href="{prefix}wiki.html"{wiki_active}>Wiki</a></li>
        <li><a href="{prefix}blog.html"{blog_active}>Blog</a></li>
        <li><a class="nav__cta" href="{prefix}index.html#download">Download</a></li>
      </ul>
      <div class="nav__actions">
        <button type="button" class="theme-toggle" id="theme-toggle" aria-label="Switch to light mode" title="Light mode">
          <svg class="theme-toggle__icon theme-toggle__icon--sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <circle cx="12" cy="12" r="4" />
            <path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41" />
          </svg>
          <svg class="theme-toggle__icon theme-toggle__icon--moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z" />
          </svg>
        </button>
        <button class="nav__toggle" id="nav-toggle" aria-expanded="false" aria-controls="nav-menu" aria-label="Toggle menu">
          <span></span><span></span><span></span>
        </button>
      </div>
    </nav>
  </header>

  <main class="blog-page">
{main}
  </main>

  <footer class="site-footer">
    <div class="container">
      <div class="footer__bar">
        <p>
          <a href="https://github.com/{REPO}" target="_blank" rel="noopener noreferrer">GitHub</a>
          ·
          <a href="{prefix}privacy.html">Privacy</a>
          ·
          <a href="{prefix}wiki.html">Wiki</a>
          ·
          <a href="{prefix}blog.html">Blog</a>
          ·
          <a href="{prefix}terms.html">Terms</a>
          ·
          <a href="{prefix}license.html">License</a>
        </p>
        <p class="footer__copy">© <span id="year"></span> NXStation. Not affiliated with Nintendo.</p>
      </div>
    </div>
    <button type="button" class="back-to-top" id="back-to-top" aria-label="Back to top" hidden>↑</button>
  </footer>

  <script src="{prefix}cookies.js" defer></script>
  <script src="{prefix}script.js" defer></script>
</body>
</html>
"""


def build_post(release: dict) -> None:
    tag = release["tag_name"]
    slug = tag.lstrip("v")
    title = release.get("name") or tag
    date_iso = release.get("published_at") or parse_changelog_date(tag)
    if release.get("draft_local") and not release.get("published_at"):
        date_iso = parse_changelog_date(tag)
    date_label = format_date(date_iso)
    body_html = markdown_to_html(release.get("body", ""))
    gh_url = release.get("html_url", f"https://github.com/{REPO}/releases/tag/{tag}")
    nro_url = nro_download_url(release)
    is_draft = release.get("draft_local") and not release.get("published_at")
    draft_badge = ""
    if is_draft and release.get("_latest_draft"):
        draft_badge = '<span class="blog-card__badge">Latest draft</span>'

    buttons = []
    if nro_url:
        buttons.append(
            f'<a class="btn btn--primary" href="{html.escape(nro_url)}" target="_blank" rel="noopener noreferrer">Download NXStation.nro</a>'
        )
    gh_label = "GitHub Releases" if is_draft else "View on GitHub"
    gh_href = f"https://github.com/{REPO}/releases" if is_draft else gh_url
    buttons.append(
        f'<a class="btn btn--ghost" href="{html.escape(gh_href)}" target="_blank" rel="noopener noreferrer">{gh_label}</a>'
    )
    draft_note = (
        '<p class="blog-post__draft-note">Documented here ahead of the GitHub release — check GitHub Releases for the downloadable NRO when it ships.</p>'
        if is_draft
        else ""
    )
    download_block = f"""
          <div class="blog-post__actions">
            {"".join(buttons)}
          </div>
          {draft_note}"""

    main = f"""    <article class="container blog-page__inner blog-post">
      <nav class="blog-breadcrumb" aria-label="Breadcrumb">
        <a href="../blog.html">Blog</a>
        <span aria-hidden="true">/</span>
        <span>{html.escape(tag)}</span>
      </nav>
      <header class="blog-post__header">
        <p class="eyebrow">Release notes</p>
        <h1>{html.escape(title)}</h1>
        <p class="blog-post__meta">
          <time datetime="{html.escape(date_iso or '')}">{html.escape(date_label)}</time>
          ·
          <span class="blog-post__tag">{html.escape(tag)}</span>
          {draft_badge}
        </p>
        {download_block}
      </header>
      <div class="blog-prose glass">
        {body_html}
      </div>
      <p class="blog-post__back"><a href="../blog.html">← All posts</a></p>
    </article>"""

    BLOG_DIR.mkdir(parents=True, exist_ok=True)
    (BLOG_DIR / f"{slug}.html").write_text(
        page_shell(
            title=f"{title} · NXStation Blog",
            description=excerpt_from_body(release.get("body", "")),
            depth=1,
            active="blog",
            main=main,
        ),
        encoding="utf-8",
    )


def build_index(releases: list[dict]) -> None:
    cards = []
    latest_draft_tag = next(
        (r["tag_name"] for r in releases if r.get("draft_local") and not r.get("published_at")),
        None,
    )
    for release in releases:
        tag = release["tag_name"]
        slug = tag.lstrip("v")
        title = release.get("name") or tag
        date_iso = release.get("published_at") or parse_changelog_date(tag)
        date_label = format_date(date_iso)
        excerpt = excerpt_from_body(release.get("body", ""))
        badge = ""
        if tag == latest_draft_tag:
            badge = '<span class="blog-card__badge">Latest draft</span>'
        gh_link = release.get("html_url", "")
        if release.get("draft_local") and not release.get("published_at"):
            gh_link = f"https://github.com/{REPO}/releases"
        cards.append(f"""
        <article class="blog-card glass">
          <div class="blog-card__meta">
            <time datetime="{html.escape(date_iso or '')}">{html.escape(date_label)}</time>
            <span class="blog-card__tag">{html.escape(tag)}</span>
            {badge}
          </div>
          <h2 class="blog-card__title"><a href="blog/{html.escape(slug)}.html">{html.escape(title)}</a></h2>
          <p class="blog-card__excerpt">{html.escape(excerpt)}</p>
          <div class="blog-card__links">
            <a href="blog/{html.escape(slug)}.html">Read post</a>
            <a href="{html.escape(gh_link)}" target="_blank" rel="noopener noreferrer">GitHub release</a>
          </div>
        </article>""")

    main = f"""    <div class="container blog-page__inner">
      <p class="eyebrow">Updates</p>
      <h1>Blog</h1>
      <p class="blog-page__lead">
        Release notes mirrored from
        <a href="https://github.com/{REPO}/releases" target="_blank" rel="noopener noreferrer">GitHub Releases</a>,
        plus development updates for the NXStation Switch homebrew frontend.
      </p>
      <div class="blog-list">
        {"".join(cards)}
      </div>
    </div>"""

    (ROOT / "blog.html").write_text(
        page_shell(
            title="Blog · NXStation",
            description="NXStation release notes and development updates for the Nintendo Switch homebrew frontend.",
            depth=0,
            active="blog",
            main=main,
        ),
        encoding="utf-8",
    )


def discover_local_release_tags() -> list[str]:
    repo = ROOT.parent
    tags: list[str] = []
    for path in sorted(repo.glob("release_v*.md")):
        ver = path.stem.removeprefix("release_v")
        if ver:
            tags.append(f"v{ver}")
    return tags


def merge_releases(remote: list[dict]) -> list[dict]:
    by_tag = {r["tag_name"]: r for r in remote}
    for tag in discover_local_release_tags():
        local = extract_local_release(tag)
        if local and tag not in by_tag:
            by_tag[tag] = local
        elif local and tag in by_tag and not by_tag[tag].get("body"):
            by_tag[tag]["body"] = local["body"]
            by_tag[tag]["name"] = local.get("name", by_tag[tag].get("name"))

    def sort_key(r: dict) -> tuple:
        tag = r["tag_name"].lstrip("v")
        parts = []
        for p in tag.split("."):
            try:
                parts.append(int(p))
            except ValueError:
                parts.append(0)
        while len(parts) < 3:
            parts.append(0)
        return tuple(parts)

    return sorted(by_tag.values(), key=sort_key, reverse=True)


def main() -> None:
    remote = fetch_releases()
    releases = merge_releases(remote)
    latest_draft_tag = next(
        (r["tag_name"] for r in releases if r.get("draft_local") and not r.get("published_at")),
        None,
    )
    for release in releases:
        release["_latest_draft"] = release["tag_name"] == latest_draft_tag
        build_post(release)
    build_index(releases)
    print(f"Generated blog.html and {len(releases)} posts in {BLOG_DIR}")


if __name__ == "__main__":
    main()
