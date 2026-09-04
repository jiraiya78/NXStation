#pragma once

#include <string>

namespace sf {

inline constexpr const char* kFavoritesSystemId = "favorites";
inline constexpr const char* kLastPlayedSystemId = "lastplayed";

bool isVirtualSystemId(const std::string& systemId);
const char* virtualSystemDisplayName(const std::string& systemId);
/** Bundled background file stem (e.g. favorites → favorite.jpg). */
const char* virtualSystemBackgroundStem(const std::string& systemId);
std::string virtualSectionRoot(const std::string& systemId);

/** Short label for Favorites / Last Played rows (e.g. snes → SNES, megadrive → MEGADRIVE). */
std::string systemAcronym(const std::string& systemId);

} // namespace sf
