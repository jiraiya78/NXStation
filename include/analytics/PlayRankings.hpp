#pragma once

#include "analytics/PlaytimeTypes.hpp"

#include <string>
#include <vector>

namespace sf::analytics {

struct RankedGame {
    std::string romName;
    std::string systemId;
    uint64_t playtimeSeconds = 0;
    uint64_t launchCount = 0;
};

struct RankedSystem {
    std::string systemId;
    uint64_t playtimeSeconds = 0;
    uint64_t launchCount = 0;
    size_t uniqueGames = 0;
};

struct PlayRankingsResult {
    std::vector<RankedGame> topGamesByPlaytime;
    std::vector<RankedGame> topGamesByLaunches;
    std::vector<RankedSystem> topSystemsByPlaytime;
    std::vector<RankedSystem> topSystemsByLaunches;
};

PlayRankingsResult buildPlayRankings(const std::vector<GamePlayLog>& logs, size_t limit = 15);

} // namespace sf::analytics
