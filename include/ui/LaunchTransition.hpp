#pragma once

#include "app/Models.hpp"

#include <string>

namespace sf::ui {

/**
 * Full-screen zoom-in + fade-out (~1s), then chain-load the game via NroLauncher.
 * Safe to call from list / detail / screensaver; ignores overlapping launches.
 */
void beginGameLaunch(const SystemConfig& system, const GameItem& game);

} // namespace sf::ui
