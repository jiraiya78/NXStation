#pragma once

#include "analytics/PlaytimeTypes.hpp"

#include <vector>

namespace sf::analytics {

PlaytimeHabitsResult analyzePlaytimeHabits(const std::vector<PlaySession>& sessionHistory);

/** Convenience: load RetroArch data and run habit analysis. */
PlaytimeHabitsResult analyzePlaytimeHabitsFromLogs(const std::vector<GamePlayLog>& logs);

/** One synthetic session per game (used by analyzePlaytimeHabitsFromLogs). */
std::vector<PlaySession> buildSyntheticSessions(const std::vector<GamePlayLog>& logs);

} // namespace sf::analytics
