#pragma once

/**
 * ScreenScraper API credentials.
 *
 * Website login (ssid/sspassword) is always required — users set it in Settings
 * (saved to settings/user_screenscraper.json).
 *
 * Developer API keys (devid/devpassword) identify the app to ScreenScraper. For local
 * builds, copy ScreenScraperCredentials.local.hpp.example to
 * ScreenScraperCredentials.local.hpp and fill in your dev API registration.
 * The local file is gitignored and is not shipped in public source releases.
 */
#if defined(__has_include)
#if __has_include("ScreenScraperCredentials.local.hpp")
#include "ScreenScraperCredentials.local.hpp"
#endif
#endif

namespace sf::screenscraper {

#ifndef NXSTATION_SS_DEV_ID
inline constexpr const char* kDevId = "";
inline constexpr const char* kDevPassword = "";
#endif

/** ScreenScraper API softname — fixed in app, not user-editable. */
inline constexpr const char* kSoftName = "NXStation";

inline bool devCredentialsConfigured()
{
    return kDevId[0] != '\0' && kDevPassword[0] != '\0';
}

} // namespace sf::screenscraper
