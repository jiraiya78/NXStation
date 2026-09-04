#pragma once

#include <string>

namespace sf::ui::GameSearch {

/**
 * Opens the software keyboard, then pushes the matches as a game list.
 * @param systemId Restrict to one system, or empty to search every system.
 */
void prompt(const std::string& systemId = {});

} // namespace sf::ui::GameSearch
