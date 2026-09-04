#include "analytics/PlayRankings.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace sf::analytics {
namespace {

std::vector<RankedGame> sortGames(std::unordered_map<std::string, RankedGame> games,
                                  bool byLaunches, size_t limit)
{
    std::vector<RankedGame> out;
    out.reserve(games.size());
    for (auto& [_, game] : games)
        out.push_back(std::move(game));

    if (byLaunches) {
        std::sort(out.begin(), out.end(), [](const RankedGame& a, const RankedGame& b) {
            if (a.launchCount != b.launchCount)
                return a.launchCount > b.launchCount;
            return a.playtimeSeconds > b.playtimeSeconds;
        });
    } else {
        std::sort(out.begin(), out.end(), [](const RankedGame& a, const RankedGame& b) {
            if (a.playtimeSeconds != b.playtimeSeconds)
                return a.playtimeSeconds > b.playtimeSeconds;
            return a.launchCount > b.launchCount;
        });
    }

    if (out.size() > limit)
        out.resize(limit);
    return out;
}

std::vector<RankedSystem> sortSystems(std::unordered_map<std::string, RankedSystem> systems,
                                      bool byLaunches, size_t limit)
{
    std::vector<RankedSystem> out;
    out.reserve(systems.size());
    for (auto& [_, system] : systems)
        out.push_back(std::move(system));

    if (byLaunches) {
        std::sort(out.begin(), out.end(), [](const RankedSystem& a, const RankedSystem& b) {
            if (a.launchCount != b.launchCount)
                return a.launchCount > b.launchCount;
            return a.playtimeSeconds > b.playtimeSeconds;
        });
    } else {
        std::sort(out.begin(), out.end(), [](const RankedSystem& a, const RankedSystem& b) {
            if (a.playtimeSeconds != b.playtimeSeconds)
                return a.playtimeSeconds > b.playtimeSeconds;
            return a.launchCount > b.launchCount;
        });
    }

    if (out.size() > limit)
        out.resize(limit);
    return out;
}

} // namespace

PlayRankingsResult buildPlayRankings(const std::vector<GamePlayLog>& logs, size_t limit)
{
    std::unordered_map<std::string, RankedGame> games;
    std::unordered_map<std::string, RankedSystem> systems;
    std::unordered_map<std::string, std::unordered_set<std::string>> systemGameKeys;

    for (const auto& log : logs) {
        if (log.playtimeSeconds == 0 && log.launchCount == 0)
            continue;

        const std::string gameKey = log.romPath.empty() ? log.romName : log.romPath;
        RankedGame& game = games[gameKey];
        if (game.romName.empty())
            game.romName = log.romName.empty() ? gameKey : log.romName;
        if (game.systemId.empty())
            game.systemId = log.systemId;
        game.playtimeSeconds += log.playtimeSeconds;
        game.launchCount += log.launchCount;

        const std::string sysId = log.systemId.empty() ? "unknown" : log.systemId;
        RankedSystem& system = systems[sysId];
        system.systemId = sysId;
        system.playtimeSeconds += log.playtimeSeconds;
        system.launchCount += log.launchCount;
        systemGameKeys[sysId].insert(gameKey);
    }

    for (auto& [sysId, system] : systems)
        system.uniqueGames = systemGameKeys[sysId].size();

    PlayRankingsResult result;
    result.topGamesByPlaytime = sortGames(games, false, limit);
    result.topGamesByLaunches = sortGames(games, true, limit);
    result.topSystemsByPlaytime = sortSystems(systems, false, limit);
    result.topSystemsByLaunches = sortSystems(systems, true, limit);
    return result;
}

} // namespace sf::analytics
