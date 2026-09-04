#pragma once

#include "analytics/PlaytimeTypes.hpp"

#include <vector>

namespace sf::analytics {

PersonalityMetricsResult calculatePersonalityMetrics(const std::vector<GamePlayLog>& gameLogData);

} // namespace sf::analytics
