#pragma once

#include "analytics/PlaytimeTypes.hpp"

#include <string>
#include <vector>

namespace sf::analytics {

/** Default RetroArch root on Switch (playlists + runtime logs). */
inline constexpr const char* kDefaultRetroArchDir = "sdmc:/retroarch";

/** Load play logs from RetroArch .lrtl runtime files and .lpl playlists. */
std::vector<GamePlayLog> loadRetroArchPlayLogs(const std::string& retroArchDir = kDefaultRetroArchDir);

} // namespace sf::analytics
