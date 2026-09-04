#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace sf::analytics {

/** One RetroArch-tracked title with aggregate runtime. */
struct GamePlayLog {
    std::string romPath;
    std::string romName;
    std::string systemId;   // NXStation id when resolved (e.g. snes)
    std::string systemName; // RetroArch playlist / db_name label
    std::string coreName;
    uint64_t playtimeSeconds = 0;
    uint64_t launchCount = 0;    // NXStation launches when tracked
    uint64_t lastPlayedUnix = 0; // 0 if unknown
    int releaseYear = 0;         // from metadata or system map
    std::string genre;           // from NXStation / gamelist metadata when available
};

/** Synthetic or inferred session used for habit analytics. */
struct PlaySession {
    std::string romPath;
    std::string romName;
    std::string systemId;
    uint64_t durationSeconds = 0;
    uint64_t timestampUnix = 0; // session start (UTC)
};

struct DecadeBreakdown {
    float gen70s = 0.f;
    float gen80s = 0.f;
    float gen90s = 0.f;
    float gen2000s = 0.f;
    float unknown = 0.f;
};

struct PersonalityMetricsResult {
    std::string primaryTag;
    std::string tagDescription;
    DecadeBreakdown decadeBreakdown;
    std::vector<std::string> decadeLines; // formatted strings for UI
    std::vector<std::string> timeWarpStats;
    uint64_t totalPlaytimeSeconds = 0;
    size_t uniqueGames = 0;
    bool hasData = false;
    std::string emptyReason;
};

struct HeatmapDay {
    std::string date; // YYYY-MM-DD
    int intensity = 0; // 0-4
    uint64_t playSeconds = 0;
};

struct TimeOfDaySlice {
    std::string label;
    float percent = 0.f;
};

struct PlaytimeHabitsResult {
    bool hasData = false;
    std::string emptyReason;
    uint64_t totalPlaytimeSeconds = 0;
    size_t totalSessionCount = 0;
    double averageSessionSeconds = 0.0;
    std::string gamingStyle;
    std::vector<HeatmapDay> heatmap365;
    std::vector<TimeOfDaySlice> timeOfDay;
    std::string timeOfDayArchetype;
    float backlogDustScore = 0.f;
    size_t abandonedGamesCount = 0;
    size_t uniqueGamesLaunched = 0;
    std::vector<std::string> abandonedGameNames;
    std::vector<std::string> summaryLines;
};

} // namespace sf::analytics
