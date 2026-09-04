#!/usr/bin/env python3
"""Generate bundled theme XML files under resources/themes/."""

from __future__ import annotations

import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "resources" / "themes"

# Base accent palettes — dark and light pairs share hue family.
PALETTES = {
  # --- Dark ---
  "Vampire": {
    "variant": "dark",
    "accent": "#50FA7B", "highlight2": "#BD93F9",
    "clear": "#120A1C", "background": "#160C22", "backdrop": "#0C0614B8",
    "highlight_bg": "#301C48", "sidebar_bg": "#10081A", "separator": "#442C60",
    "header_subtitle": "#A08CC8", "list_value": "#A08CC8",
    "primary_btn": "#622AA8", "primary_disabled_bg": "#281C38", "default_btn_bg": "#241634",
    "text": "#F8F8F2", "text_disabled": "#625876",
    "card_bg": "#241238", "card_border": "#442C60",
    "dialog_bg": "#0A0612", "dialog_border": "#50FA7BD4", "dialog_row": "#1C102A",
    "dialog_backdrop": "#000000A0",
    "title_text": "#ECEEF2", "detail_text": "#A5ACC4", "muted_text": "#8C91A0", "body_text": "#C8CDDC",
    "preview_text": "#D8D0E8", "hint_text": "#7868A0", "count_text": "#A090C8",
    "log_success": "#50FA7B", "log_failure": "#FF5555", "log_progress": "#BD93F9", "log_neutral": "#CCCCCC",
    "overlay_bg": "#0C0614", "favorite_text": "#FFFFFF",
  },
  "Abyss": {
    "variant": "dark",
    "accent": "#5B9FFF", "highlight2": "#7CB9FF",
    "clear": "#0A1020", "background": "#0E1428", "backdrop": "#080C18B8",
    "highlight_bg": "#1A2848", "sidebar_bg": "#0A0E1C", "separator": "#2A3A60",
    "header_subtitle": "#8CA8D8", "list_value": "#8CA8D8",
    "primary_btn": "#2A4A98", "primary_disabled_bg": "#1A2238", "default_btn_bg": "#141C30",
    "text": "#F0F4FC", "text_disabled": "#5A6888",
    "card_bg": "#141E34", "card_border": "#2A3A60",
    "dialog_bg": "#080C18", "dialog_border": "#5B9FFFD4", "dialog_row": "#121C30",
    "dialog_backdrop": "#000000A0",
    "title_text": "#E8EEF8", "detail_text": "#9CB0D8", "muted_text": "#7888A8", "body_text": "#B8C8E0",
    "preview_text": "#C8D8F0", "hint_text": "#6880A8", "count_text": "#88A0C8",
    "log_success": "#5B9FFF", "log_failure": "#FF6B6B", "log_progress": "#7CB9FF", "log_neutral": "#B0B8C8",
    "overlay_bg": "#080C18", "favorite_text": "#FFFFFF",
  },
  "Sunfire": {
    "variant": "dark",
    "accent": "#FF9F43", "highlight2": "#FFB347",
    "clear": "#1C1008", "background": "#221408", "backdrop": "#140A04B8",
    "highlight_bg": "#482818", "sidebar_bg": "#1A0E06", "separator": "#604028",
    "header_subtitle": "#D8B088", "list_value": "#D8B088",
    "primary_btn": "#984818", "primary_disabled_bg": "#301C10", "default_btn_bg": "#28180C",
    "text": "#FFF8F0", "text_disabled": "#886858",
    "card_bg": "#301C10", "card_border": "#604028",
    "dialog_bg": "#140A04", "dialog_border": "#FF9F43D4", "dialog_row": "#241408",
    "dialog_backdrop": "#000000A0",
    "title_text": "#F8F0E8", "detail_text": "#D0B090", "muted_text": "#A08060", "body_text": "#E0C8A8",
    "preview_text": "#E8D0B0", "hint_text": "#A07048", "count_text": "#C89868",
    "log_success": "#FFB347", "log_failure": "#FF5555", "log_progress": "#FF9F43", "log_neutral": "#C8B0A0",
    "overlay_bg": "#140A04", "favorite_text": "#FFFFFF",
  },
  "Carnage": {
    "variant": "dark",
    "accent": "#FF5555", "highlight2": "#FF7A7A",
    "clear": "#1C0A0C", "background": "#220E10", "backdrop": "#140608B8",
    "highlight_bg": "#481820", "sidebar_bg": "#1A080A", "separator": "#602830",
    "header_subtitle": "#D88898", "list_value": "#D88898",
    "primary_btn": "#982028", "primary_disabled_bg": "#301018", "default_btn_bg": "#281014",
    "text": "#FFF0F0", "text_disabled": "#886068",
    "card_bg": "#301018", "card_border": "#602830",
    "dialog_bg": "#140608", "dialog_border": "#FF5555D4", "dialog_row": "#240810",
    "dialog_backdrop": "#000000A0",
    "title_text": "#F8E8E8", "detail_text": "#D098A0", "muted_text": "#A06870", "body_text": "#E0B0B8",
    "preview_text": "#E8B8C0", "hint_text": "#A04858", "count_text": "#C87888",
    "log_success": "#FF7A7A", "log_failure": "#FF3333", "log_progress": "#FF5555", "log_neutral": "#C8A0A8",
    "overlay_bg": "#140608", "favorite_text": "#FFFFFF",
  },
  "Glacial": {
    "variant": "dark",
    "accent": "#56E8E8", "highlight2": "#8BEFFD",
    "clear": "#081820", "background": "#0C1E28", "backdrop": "#061014B8",
    "highlight_bg": "#183848", "sidebar_bg": "#08141A", "separator": "#285060",
    "header_subtitle": "#88C8D8", "list_value": "#88C8D8",
    "primary_btn": "#187888", "primary_disabled_bg": "#102830", "default_btn_bg": "#0C2030",
    "text": "#E8FAFA", "text_disabled": "#588898",
    "card_bg": "#102830", "card_border": "#285060",
    "dialog_bg": "#061014", "dialog_border": "#56E8E8D4", "dialog_row": "#0C1C28",
    "dialog_backdrop": "#000000A0",
    "title_text": "#E0F8F8", "detail_text": "#88C0D0", "muted_text": "#6098A8", "body_text": "#A8D8E0",
    "preview_text": "#B8E8F0", "hint_text": "#4898A8", "count_text": "#68B8C8",
    "log_success": "#56E8E8", "log_failure": "#FF6B6B", "log_progress": "#8BEFFD", "log_neutral": "#A0C0C8",
    "overlay_bg": "#061014", "favorite_text": "#FFFFFF",
  },
  "Mirage": {
    "variant": "dark",
    "accent": "#D4AF37", "highlight2": "#E6C84A",
    "clear": "#1A1608", "background": "#201C0C", "backdrop": "#121004B8",
    "highlight_bg": "#484018", "sidebar_bg": "#161206", "separator": "#605828",
    "header_subtitle": "#D8C888", "list_value": "#D8C888",
    "primary_btn": "#887018", "primary_disabled_bg": "#302810", "default_btn_bg": "#28200C",
    "text": "#FFF8E8", "text_disabled": "#887848",
    "card_bg": "#302810", "card_border": "#605828",
    "dialog_bg": "#121004", "dialog_border": "#D4AF37D4", "dialog_row": "#221C08",
    "dialog_backdrop": "#000000A0",
    "title_text": "#F8F0D8", "detail_text": "#D0C088", "muted_text": "#A09058", "body_text": "#E0D0A0",
    "preview_text": "#E8D8A8", "hint_text": "#A08038", "count_text": "#C8B068",
    "log_success": "#E6C84A", "log_failure": "#FF6B6B", "log_progress": "#D4AF37", "log_neutral": "#C8B890",
    "overlay_bg": "#121004", "favorite_text": "#FFFFFF",
  },
  "Cyberpunk": {
    "variant": "dark",
    "accent": "#FF79C6", "highlight2": "#FF92DF",
    "clear": "#1C0A18", "background": "#220E1E", "backdrop": "#140610B8",
    "highlight_bg": "#481838", "sidebar_bg": "#1A0816", "separator": "#602848",
    "header_subtitle": "#D888C8", "list_value": "#D888C8",
    "primary_btn": "#982868", "primary_disabled_bg": "#301020", "default_btn_bg": "#28101C",
    "text": "#FFF0F8", "text_disabled": "#886078",
    "card_bg": "#301020", "card_border": "#602848",
    "dialog_bg": "#140610", "dialog_border": "#FF79C6D4", "dialog_row": "#240818",
    "dialog_backdrop": "#000000A0",
    "title_text": "#F8E8F0", "detail_text": "#D098C0", "muted_text": "#A06888", "body_text": "#E0B0D0",
    "preview_text": "#E8B8D8", "hint_text": "#A04878", "count_text": "#C878A8",
    "log_success": "#FF92DF", "log_failure": "#FF5555", "log_progress": "#FF79C6", "log_neutral": "#C8A0B8",
    "overlay_bg": "#140610", "favorite_text": "#FFFFFF",
  },
  "Warlock": {
    "variant": "dark",
    "accent": "#C75C5C", "highlight2": "#A84848",
    "clear": "#18080A", "background": "#1E0C0E", "backdrop": "#100406B8",
    "highlight_bg": "#401820", "sidebar_bg": "#140608", "separator": "#582028",
    "header_subtitle": "#C88888", "list_value": "#C88888",
    "primary_btn": "#781820", "primary_disabled_bg": "#280810", "default_btn_bg": "#20080C",
    "text": "#F8E8E8", "text_disabled": "#785858",
    "card_bg": "#280810", "card_border": "#582028",
    "dialog_bg": "#100406", "dialog_border": "#C75C5CD4", "dialog_row": "#1C0808",
    "dialog_backdrop": "#000000A0",
    "title_text": "#F0E0E0", "detail_text": "#C09090", "muted_text": "#906060", "body_text": "#D8A8A8",
    "preview_text": "#D8A0A0", "hint_text": "#904040", "count_text": "#B07070",
    "log_success": "#C75C5C", "log_failure": "#FF4444", "log_progress": "#A84848", "log_neutral": "#C0A0A0",
    "overlay_bg": "#100406", "favorite_text": "#FFFFFF",
  },
  "Toxic": {
    "variant": "dark",
    "accent": "#50FA7B", "highlight2": "#69FF94",
    "clear": "#081810", "background": "#0C1E14", "backdrop": "#06100AB8",
    "highlight_bg": "#183828", "sidebar_bg": "#08140C", "separator": "#285840",
    "header_subtitle": "#88C8A0", "list_value": "#88C8A0",
    "primary_btn": "#187838", "primary_disabled_bg": "#102818", "default_btn_bg": "#0C2018",
    "text": "#E8FFF0", "text_disabled": "#588868",
    "card_bg": "#102818", "card_border": "#285840",
    "dialog_bg": "#06100A", "dialog_border": "#50FA7BD4", "dialog_row": "#0C1C14",
    "dialog_backdrop": "#000000A0",
    "title_text": "#E0F8E8", "detail_text": "#88C0A0", "muted_text": "#609878", "body_text": "#A8D8B8",
    "preview_text": "#B8E8C8", "hint_text": "#489860", "count_text": "#68B880",
    "log_success": "#50FA7B", "log_failure": "#FF6B6B", "log_progress": "#69FF94", "log_neutral": "#A0C0A8",
    "overlay_bg": "#06100A", "favorite_text": "#FFFFFF",
  },
  # --- Light ---
  "Horizon": {
    "variant": "light",
    "accent": "#2563EB", "highlight2": "#3B82F6",
    "clear": "#F5F8FD", "background": "#EEF2FA", "backdrop": "#1A284040",
    "highlight_bg": "#DCE6F8", "sidebar_bg": "#E4ECF8", "separator": "#B8C8E0",
    "header_subtitle": "#4A6088", "list_value": "#4A6088",
    "primary_btn": "#2563EB", "primary_disabled_bg": "#C8D4E8", "default_btn_bg": "#D8E2F0",
    "text": "#152238", "text_disabled": "#8898B0",
    "card_bg": "#FFFFFF", "card_border": "#C0D0E8",
    "dialog_bg": "#FFFFFF", "dialog_border": "#2563EBC0", "dialog_row": "#E8EEF8",
    "dialog_backdrop": "#15223860",
    "title_text": "#101828", "detail_text": "#3A5070", "muted_text": "#607088", "body_text": "#4A5870",
    "preview_text": "#2A4060", "hint_text": "#607898", "count_text": "#4A6898",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#2563EB", "log_neutral": "#6B7280",
    "overlay_bg": "#EEF2FA", "favorite_text": "#152238",
  },
  "Solstice": {
    "variant": "light",
    "accent": "#D97706", "highlight2": "#F59E0B",
    "clear": "#FFFAF5", "background": "#FFF5EB", "backdrop": "#2A1A0840",
    "highlight_bg": "#FFE8CC", "sidebar_bg": "#FFF0E0", "separator": "#E8C8A0",
    "header_subtitle": "#8A6040", "list_value": "#8A6040",
    "primary_btn": "#D97706", "primary_disabled_bg": "#E8D0B8", "default_btn_bg": "#F0E0C8",
    "text": "#2A1A08", "text_disabled": "#A08060",
    "card_bg": "#FFFFFF", "card_border": "#E8C8A0",
    "dialog_bg": "#FFFFFF", "dialog_border": "#D97706C0", "dialog_row": "#FFF0E0",
    "dialog_backdrop": "#2A1A0860",
    "title_text": "#1A1008", "detail_text": "#6A4830", "muted_text": "#907050", "body_text": "#705840",
    "preview_text": "#503820", "hint_text": "#A07040", "count_text": "#B08048",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#D97706", "log_neutral": "#78716C",
    "overlay_bg": "#FFF5EB", "favorite_text": "#2A1A08",
  },
  "Prism": {
    "variant": "light",
    "accent": "#DC2626", "highlight2": "#EF4444",
    "clear": "#FFF8F8", "background": "#FFF0F0", "backdrop": "#2A101040",
    "highlight_bg": "#FFE0E0", "sidebar_bg": "#FFE8E8", "separator": "#E8B0B0",
    "header_subtitle": "#8A4848", "list_value": "#8A4848",
    "primary_btn": "#DC2626", "primary_disabled_bg": "#E8C0C0", "default_btn_bg": "#F0D0D0",
    "text": "#2A1010", "text_disabled": "#A07070",
    "card_bg": "#FFFFFF", "card_border": "#E8B0B0",
    "dialog_bg": "#FFFFFF", "dialog_border": "#DC2626C0", "dialog_row": "#FFE8E8",
    "dialog_backdrop": "#2A101060",
    "title_text": "#1A0808", "detail_text": "#6A3838", "muted_text": "#906060", "body_text": "#704040",
    "preview_text": "#502020", "hint_text": "#A05050", "count_text": "#B06060",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#EF4444", "log_neutral": "#78716C",
    "overlay_bg": "#FFF0F0", "favorite_text": "#2A1010",
  },
  "Iceberg": {
    "variant": "light",
    "accent": "#0891B2", "highlight2": "#06B6D4",
    "clear": "#F5FCFC", "background": "#ECFAFA", "backdrop": "#0A282840",
    "highlight_bg": "#D0F0F0", "sidebar_bg": "#E0F5F5", "separator": "#A8D8E0",
    "header_subtitle": "#3A6878", "list_value": "#3A6878",
    "primary_btn": "#0891B2", "primary_disabled_bg": "#B8D8E0", "default_btn_bg": "#C8E8F0",
    "text": "#0A2828", "text_disabled": "#6898A0",
    "card_bg": "#FFFFFF", "card_border": "#A8D8E0",
    "dialog_bg": "#FFFFFF", "dialog_border": "#0891B2C0", "dialog_row": "#E0F5F5",
    "dialog_backdrop": "#0A282860",
    "title_text": "#081E1E", "detail_text": "#2A5860", "muted_text": "#508088", "body_text": "#406870",
    "preview_text": "#205058", "hint_text": "#5098A0", "count_text": "#48A0B0",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#0891B2", "log_neutral": "#6B7280",
    "overlay_bg": "#ECFAFA", "favorite_text": "#0A2828",
  },
  "Sunshine": {
    "variant": "light",
    "accent": "#B8860B", "highlight2": "#D4A017",
    "clear": "#FFFDF5", "background": "#FFFAEB", "backdrop": "#2A240840",
    "highlight_bg": "#FFF0C8", "sidebar_bg": "#FFF5D8", "separator": "#E8D090",
    "header_subtitle": "#8A7840", "list_value": "#8A7840",
    "primary_btn": "#B8860B", "primary_disabled_bg": "#E8D8A8", "default_btn_bg": "#F0E8C0",
    "text": "#2A2408", "text_disabled": "#A09058",
    "card_bg": "#FFFFFF", "card_border": "#E8D090",
    "dialog_bg": "#FFFFFF", "dialog_border": "#B8860BC0", "dialog_row": "#FFF5D8",
    "dialog_backdrop": "#2A240860",
    "title_text": "#1A1808", "detail_text": "#6A5830", "muted_text": "#908050", "body_text": "#706840",
    "preview_text": "#504820", "hint_text": "#A09040", "count_text": "#B0A048",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#B8860B", "log_neutral": "#78716C",
    "overlay_bg": "#FFFAEB", "favorite_text": "#2A2408",
  },
  "Flamingo": {
    "variant": "light",
    "accent": "#DB2777", "highlight2": "#EC4899",
    "clear": "#FFF8FC", "background": "#FFF0F8", "backdrop": "#2A102040",
    "highlight_bg": "#FFE0F0", "sidebar_bg": "#FFE8F4", "separator": "#E8B0D0",
    "header_subtitle": "#8A4868", "list_value": "#8A4868",
    "primary_btn": "#DB2777", "primary_disabled_bg": "#E8C0D8", "default_btn_bg": "#F0D0E8",
    "text": "#2A1020", "text_disabled": "#A07088",
    "card_bg": "#FFFFFF", "card_border": "#E8B0D0",
    "dialog_bg": "#FFFFFF", "dialog_border": "#DB2777C0", "dialog_row": "#FFE8F4",
    "dialog_backdrop": "#2A102060",
    "title_text": "#1A0818", "detail_text": "#6A3858", "muted_text": "#906878", "body_text": "#704860",
    "preview_text": "#502840", "hint_text": "#A05078", "count_text": "#B06088",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#DB2777", "log_neutral": "#78716C",
    "overlay_bg": "#FFF0F8", "favorite_text": "#2A1020",
  },
  "Clay": {
    "variant": "light",
    "accent": "#991B1B", "highlight2": "#B91C1C",
    "clear": "#FCF8F8", "background": "#FAF0F0", "backdrop": "#28101040",
    "highlight_bg": "#F0D8D8", "sidebar_bg": "#F5E0E0", "separator": "#D8A8A8",
    "header_subtitle": "#784848", "list_value": "#784848",
    "primary_btn": "#991B1B", "primary_disabled_bg": "#E0C0C0", "default_btn_bg": "#ECD0D0",
    "text": "#281010", "text_disabled": "#987070",
    "card_bg": "#FFFFFF", "card_border": "#D8A8A8",
    "dialog_bg": "#FFFFFF", "dialog_border": "#991B1BC0", "dialog_row": "#F5E0E0",
    "dialog_backdrop": "#28101060",
    "title_text": "#1A0808", "detail_text": "#603838", "muted_text": "#886060", "body_text": "#684848",
    "preview_text": "#482828", "hint_text": "#985050", "count_text": "#A86060",
    "log_success": "#16A34A", "log_failure": "#991B1B", "log_progress": "#B91C1C", "log_neutral": "#78716C",
    "overlay_bg": "#FAF0F0", "favorite_text": "#281010",
  },
  "Meadow": {
    "variant": "light",
    "accent": "#16A34A", "highlight2": "#22C55E",
    "clear": "#F5FCF7", "background": "#EEFAF2", "backdrop": "#0A281840",
    "highlight_bg": "#D0F0D8", "sidebar_bg": "#E0F5E8", "separator": "#A8D8B8",
    "header_subtitle": "#3A6848", "list_value": "#3A6848",
    "primary_btn": "#16A34A", "primary_disabled_bg": "#B8E0C8", "default_btn_bg": "#C8ECD0",
    "text": "#0A2818", "text_disabled": "#689878",
    "card_bg": "#FFFFFF", "card_border": "#A8D8B8",
    "dialog_bg": "#FFFFFF", "dialog_border": "#16A34AC0", "dialog_row": "#E0F5E8",
    "dialog_backdrop": "#0A281860",
    "title_text": "#081E10", "detail_text": "#2A5840", "muted_text": "#508060", "body_text": "#406850",
    "preview_text": "#205030", "hint_text": "#509860", "count_text": "#48A868",
    "log_success": "#16A34A", "log_failure": "#DC2626", "log_progress": "#22C55E", "log_neutral": "#6B7280",
    "overlay_bg": "#EEFAF2", "favorite_text": "#0A2818",
  },
}

LEGACY_NAMES = {
    "blue": "Abyss",
    "orange": "Sunfire",
    "red": "Carnage",
    "cyan": "Glacial",
    "mustard": "Mirage",
    "pink": "Cyberpunk",
    "maroon": "Warlock",
    "green": "Toxic",
}

BRLS_KEYS = [
    "clear", "background", "backdrop", "text", "text_disabled", "accent",
    "highlight_bg", "highlight2", "sidebar_bg", "separator", "header_subtitle",
    "list_value", "primary_btn", "primary_disabled_bg", "default_btn_bg",
]

NX_KEYS = [
    "card_bg", "card_border", "dialog_bg", "dialog_border", "dialog_row", "dialog_backdrop",
    "title_text", "detail_text", "muted_text", "body_text", "preview_text", "hint_text",
    "count_text", "log_success", "log_failure", "log_progress", "log_neutral",
    "overlay_bg", "favorite_text",
]

BRLS_MAP = {
    "clear": "brls/clear",
    "background": "brls/background",
    "backdrop": "brls/backdrop",
    "text": "brls/text",
    "text_disabled": "brls/text_disabled",
    "accent": "brls/accent",
    "highlight_bg": "brls/highlight/background",
    "highlight2": "brls/highlight/color2",
    "sidebar_bg": "brls/sidebar/background",
    "separator": "brls/sidebar/separator",
    "header_subtitle": "brls/header/subtitle",
    "list_value": "brls/list/listItem_value_color",
    "primary_btn": "brls/button/primary_enabled_background",
    "primary_disabled_bg": "brls/button/primary_disabled_background",
    "default_btn_bg": "brls/button/default_enabled_background",
}


def theme_xml(name: str, palette: dict) -> str:
    variant = palette["variant"]
    accent = palette["accent"]
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<theme name="{name}" variant="{variant}">',
        "  <colors>",
    ]
    for key in BRLS_KEYS:
        brls_key = BRLS_MAP[key]
        value = palette[key]
        lines.append(f'    <color key="{brls_key}" value="{value}"/>')
    lines.append(f'    <color key="brls/highlight/color1" value="{accent}"/>')
    lines.append(f'    <color key="brls/sidebar/active_item" value="{accent}"/>')
    lines.append(f'    <color key="brls/header/border" value="{palette["separator"]}"/>')
    lines.append(f'    <color key="brls/header/rectangle" value="{palette["highlight2"]}"/>')
    lines.append(f'    <color key="brls/button/primary_enabled_text" value="{palette["text"]}"/>')
    lines.append(f'    <color key="brls/button/primary_disabled_text" value="{palette["text_disabled"]}"/>')
    lines.append(f'    <color key="brls/button/default_enabled_text" value="{palette["text"]}"/>')
    for key in NX_KEYS:
        lines.append(f'    <color key="nxstation/{key}" value="{palette[key]}"/>')
    lines.extend(["  </colors>", "</theme>", ""])
    return "\n".join(lines)


def main() -> None:
    # Remove legacy theme folders.
    for legacy in LEGACY_NAMES:
        legacy_dir = ROOT / legacy
        if legacy_dir.is_dir():
            shutil.rmtree(legacy_dir)
            print(f"removed legacy {legacy_dir}")

    for name, palette in PALETTES.items():
        folder = ROOT / name
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "theme.xml"
        path.write_text(theme_xml(name, palette), encoding="utf-8")
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
