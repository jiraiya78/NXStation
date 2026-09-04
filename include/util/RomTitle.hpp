#pragma once

#include <string>

namespace sf::RomTitle {

/** Build a display title from a ROM filename stem (no extension). */
std::string fromStem(const std::string& stem);

/** Build a display title from a full ROM path. */
std::string fromPath(const std::string& path);

} // namespace sf::RomTitle
