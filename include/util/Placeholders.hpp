#pragma once

#include <string>

namespace sf::paths {

/** User override for missing game box art. */
inline constexpr const char* USER_GAME_ART_PLACEHOLDER =
    "sdmc:/switch/NXStation/data/resources/nxstation_box.png";

/** User override for missing system carousel art. */
inline constexpr const char* USER_SYSTEM_ART_PLACEHOLDER =
    "sdmc:/switch/NXStation/data/resources/nxstation.jpg";

/** Bundled romfs path (no prefix) for missing game box art. */
inline constexpr const char* BUNDLED_GAME_ART_PLACEHOLDER = "img/systems/nxstation_box.png";

/** Bundled romfs path (no prefix) for missing system carousel art. */
inline constexpr const char* BUNDLED_SYSTEM_ART_PLACEHOLDER = "img/nxstation.jpg";

} // namespace sf::paths

namespace sf {

/** Resolved SD path if a user override exists; empty → use romfs resource. */
std::string gameArtPlaceholderPath();
std::string systemArtPlaceholderPath();

/** Romfs resource id (no romfs: prefix) for bundled placeholders. */
const char* gameArtPlaceholderRes();
const char* systemArtPlaceholderRes();

} // namespace sf
