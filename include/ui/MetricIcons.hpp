#pragma once

#include <string>

namespace sf::ui {

/** romfs resource path for a personality gamer-tag icon (e.g. img/metrics/16bit_purist.png). */
std::string personalityTagIconRes(const std::string& primaryTag);

/** romfs resource path for a gaming-style hero icon. */
std::string gamingStyleIconRes(const std::string& gamingStyle);

/** romfs resource path for a named analytics section (e.g. "overview", "heatmap"). */
std::string analyticsSectionIconRes(const std::string& sectionKey);

} // namespace sf::ui
