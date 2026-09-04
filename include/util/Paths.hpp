#pragma once



#include <string>



namespace sf::paths {



inline constexpr const char* APP_ROOT        = "sdmc:/switch/NXStation";

inline constexpr const char* DATA_DIR        = "sdmc:/switch/NXStation/data";

inline constexpr const char* SETTINGS_DIR    = "sdmc:/switch/NXStation/settings";

inline constexpr const char* CACHE_DIR       = "sdmc:/switch/NXStation/data/cache";

inline constexpr const char* META_DIR        = "sdmc:/switch/NXStation/data/meta"; // legacy scraper cache

inline constexpr const char* ARTWORK_DIR     = "sdmc:/switch/NXStation/data/artwork"; // legacy

inline constexpr const char* VIDEO_DIR       = "sdmc:/switch/NXStation/data/video"; // legacy

inline constexpr const char* BACKGROUNDS_DIR = "sdmc:/switch/NXStation/data/backgrounds";

inline constexpr const char* USER_RESOURCES_DIR = "sdmc:/switch/NXStation/data/resources";

inline constexpr const char* THEME_DIR = "sdmc:/switch/NXStation/data/theme";

inline constexpr const char* LOG_DIR = "sdmc:/switch/NXStation/log";

inline constexpr const char* BUNDLED_THEMES_RES = "themes";

inline constexpr const char* CONFIG_PATH     = "sdmc:/switch/NXStation/settings/roms_config.json";

inline constexpr const char* CONFIG_FALLBACK = "romfs:/roms_config.json";

inline constexpr const char* LOG_PATH        = "sdmc:/switch/NXStation/log/NXStation.log";

inline constexpr const char* BOOT_LOG_PATH   = "sdmc:/switch/NXStation/log/boot.log";

inline constexpr const char* SCRAPE_LOG_PATH = "sdmc:/switch/NXStation/log/scrape.log";

inline constexpr const char* CLOUD_LOG_PATH = "sdmc:/switch/NXStation/log/cloud.log";

inline constexpr const char* CRASH_LOG_PATH  = "sdmc:/switch/NXStation/log/crash.log";

inline constexpr const char* APP_NRO         = "sdmc:/switch/NXStation/NXStation.nro";

inline constexpr const char* USER_CORES_PATH = "sdmc:/switch/NXStation/settings/user_cores.json";

inline constexpr const char* USER_SCREENSCRAPER_PATH =

    "sdmc:/switch/NXStation/settings/user_screenscraper.json";

inline constexpr const char* USER_FAVORITES_PATH = "sdmc:/switch/NXStation/settings/user_favorites.json";

inline constexpr const char* USER_LAST_PLAYED_PATH = "sdmc:/switch/NXStation/settings/user_last_played.json";

inline constexpr const char* FAVORITES_SECTION_DIR = "sdmc:/switch/NXStation/data/favorites";

inline constexpr const char* LAST_PLAYED_SECTION_DIR = "sdmc:/switch/NXStation/data/lastplayed";

inline constexpr const char* RETURN_CHAIN_PATH = "sdmc:/switch/NXStation/return_to_nxstation.txt";

inline constexpr const char* NAV_STATE_PATH    = "sdmc:/switch/NXStation/settings/navigation_state.json";

inline constexpr const char* PLAYTIME_PENDING_PATH =
    "sdmc:/switch/NXStation/data/playtime_pending.json";

inline constexpr const char* PLAYTIME_NXSTATION_PATH =
    "sdmc:/switch/NXStation/data/playtime_nxstation.json";

inline constexpr const char* USER_SETTINGS_PATH = "sdmc:/switch/NXStation/settings/user_settings.json";

inline constexpr const char* SYSTEM_DESCRIPTIONS_PATH =
    "sdmc:/switch/NXStation/settings/system_descriptions.json";

inline constexpr const char* SYSTEM_DESCRIPTIONS_FALLBACK = "romfs:/system_descriptions.json";

inline constexpr const char* GOOGLE_OAUTH_PATH = "sdmc:/switch/NXStation/settings/google_oauth.json";

inline constexpr const char* USER_CLOUD_PATH = "sdmc:/switch/NXStation/settings/user_cloud.json";

inline constexpr const char* CLOUD_DIR = "sdmc:/switch/NXStation/data/cloud";

inline constexpr const char* CLOUD_TEMP_ZIP = "sdmc:/switch/NXStation/data/cloud_sync.zip";

inline constexpr const char* CLOUD_RESTORE_ZIP = "sdmc:/switch/NXStation/data/cloud/restore_download.zip";

inline constexpr const char* PLACEHOLDER_IMG = "romfs:/img/placeholder.png";



} // namespace sf::paths

