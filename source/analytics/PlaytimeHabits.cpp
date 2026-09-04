#include "analytics/PlaytimeHabits.hpp"

#include "analytics/RetroArchPlaytime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <unordered_map>

namespace sf::analytics {
namespace {

constexpr uint64_t kTenMinutes = 10ULL * 60ULL;
constexpr uint64_t kThirtyDays = 30ULL * 24ULL * 3600ULL;

std::string formatDateUtc(uint64_t unixTs)
{
    if (unixTs == 0)
        return {};
    const std::time_t t = static_cast<std::time_t>(unixTs);
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &t) != 0)
        return {};
#else
    if (!gmtime_r(&t, &tm))
        return {};
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}

int hourOfDayLocal(uint64_t unixTs)
{
    if (unixTs == 0)
        return -1;
    const std::time_t t = static_cast<std::time_t>(unixTs);
    std::tm tm{};
#if defined(_WIN32)
    if (localtime_s(&tm, &t) != 0)
        return -1;
#else
    if (!localtime_r(&t, &tm))
        return -1;
#endif
    return tm.tm_hour;
}

int intensityForDailySeconds(uint64_t seconds)
{
    const uint64_t mins = seconds / 60;
    if (mins == 0)
        return 0;
    if (mins < 30)
        return 1;
    if (mins < 90)
        return 2;
    if (mins < 180)
        return 3;
    return 4;
}

std::string classifyGamingStyle(double avgSessionSeconds)
{
    if (avgSessionSeconds < 15.0 * 60.0)
        return "Micro-Burst Gamer (Bite-sized sessions)";
    if (avgSessionSeconds < 45.0 * 60.0)
        return "Casual Sessions";
    if (avgSessionSeconds < 90.0 * 60.0)
        return "Deep Dive Gamer";
    return "Marathon Runner";
}

enum class DayPart { Morning, Afternoon, Evening, Night };

DayPart classifyDayPart(int hour)
{
    if (hour < 0)
        return DayPart::Evening;
    if (hour >= 6 && hour < 12)
        return DayPart::Morning;
    if (hour >= 12 && hour < 18)
        return DayPart::Afternoon;
    if (hour >= 18 && hour < 23)
        return DayPart::Evening;
    return DayPart::Night;
}

const char* dayPartLabel(DayPart p)
{
    switch (p) {
        case DayPart::Morning:
            return "Morning";
        case DayPart::Afternoon:
            return "Afternoon";
        case DayPart::Evening:
            return "Evening";
        case DayPart::Night:
            return "Night Owl";
    }
    return "Evening";
}

uint64_t nowUnix()
{
    const std::time_t t = std::time(nullptr);
    return t > 0 ? static_cast<uint64_t>(t) : 0;
}

} // namespace

PlaytimeHabitsResult analyzePlaytimeHabits(const std::vector<PlaySession>& sessionHistory)
{
    PlaytimeHabitsResult result;

    std::vector<PlaySession> sessions;
    sessions.reserve(sessionHistory.size());
    for (const auto& s : sessionHistory) {
        if (s.durationSeconds > 0 || s.timestampUnix > 0)
            sessions.push_back(s);
    }

    if (sessions.empty()) {
        result.emptyReason =
            "No session history available. RetroArch runtime logs provide one synthetic "
            "session per title (total playtime at last played).";
        return result;
    }

    result.hasData = true;
    result.totalSessionCount = sessions.size();

    std::unordered_map<std::string, uint64_t> dailySeconds;
    std::unordered_map<std::string, uint64_t> gamePlaytime;
    std::unordered_map<std::string, uint64_t> gameLastPlayed;
    std::unordered_map<std::string, std::string> gameNames;
    uint64_t partSeconds[4] = {0, 0, 0, 0};

    for (const auto& s : sessions) {
        result.totalPlaytimeSeconds += s.durationSeconds;

        const std::string key = s.romPath.empty() ? s.romName : s.romPath;
        gamePlaytime[key] += s.durationSeconds;
        gameNames[key] = s.romName.empty() ? key : s.romName;
        if (s.timestampUnix > gameLastPlayed[key])
            gameLastPlayed[key] = s.timestampUnix;

        if (!s.timestampUnix)
            continue;

        const std::string day = formatDateUtc(s.timestampUnix);
        if (!day.empty())
            dailySeconds[day] += s.durationSeconds;

        const int hour = hourOfDayLocal(s.timestampUnix);
        const DayPart part = classifyDayPart(hour);
        partSeconds[static_cast<size_t>(part)] += s.durationSeconds;
    }

    result.uniqueGamesLaunched = gamePlaytime.size();
    result.averageSessionSeconds =
        result.totalSessionCount > 0
            ? static_cast<double>(result.totalPlaytimeSeconds) / static_cast<double>(result.totalSessionCount)
            : 0.0;
    result.gamingStyle = classifyGamingStyle(result.averageSessionSeconds);

    const uint64_t now = nowUnix();
    for (const auto& [key, seconds] : gamePlaytime) {
        const uint64_t last = gameLastPlayed[key];
        if (seconds > 0 && seconds < kTenMinutes && last > 0 && now > last
            && (now - last) > kThirtyDays) {
            result.abandonedGamesCount++;
            if (result.abandonedGameNames.size() < 24)
                result.abandonedGameNames.push_back(gameNames[key]);
        }
    }

    if (result.uniqueGamesLaunched > 0) {
        result.backlogDustScore =
            (static_cast<float>(result.abandonedGamesCount)
             / static_cast<float>(result.uniqueGamesLaunched))
            * 100.f;
    }

    // 365-day heatmap ending today (UTC dates with data only; missing days = intensity 0).
    std::vector<std::string> sortedDays;
    sortedDays.reserve(dailySeconds.size());
    for (const auto& [day, _] : dailySeconds)
        sortedDays.push_back(day);
    std::sort(sortedDays.begin(), sortedDays.end());
    const size_t keep = std::min(sortedDays.size(), size_t{365});
    const size_t start = sortedDays.size() > keep ? sortedDays.size() - keep : 0;
    for (size_t i = start; i < sortedDays.size(); ++i) {
        const auto& day = sortedDays[i];
        HeatmapDay cell;
        cell.date = day;
        cell.playSeconds = dailySeconds[day];
        cell.intensity = intensityForDailySeconds(cell.playSeconds);
        result.heatmap365.push_back(std::move(cell));
    }

    const uint64_t partTotal = partSeconds[0] + partSeconds[1] + partSeconds[2] + partSeconds[3];
    for (size_t i = 0; i < 4; ++i) {
        TimeOfDaySlice slice;
        slice.label = dayPartLabel(static_cast<DayPart>(i));
        slice.percent = partTotal > 0
                            ? (static_cast<float>(partSeconds[i]) / static_cast<float>(partTotal)) * 100.f
                            : 0.f;
        result.timeOfDay.push_back(slice);
    }

    size_t dominant = 0;
    for (size_t i = 1; i < 4; ++i) {
        if (result.timeOfDay[i].percent > result.timeOfDay[dominant].percent)
            dominant = i;
    }
    char archetype[128];
    std::snprintf(archetype, sizeof(archetype), "%.0f%% of your gaming happens during %s hours.",
                  result.timeOfDay[dominant].percent, result.timeOfDay[dominant].label.c_str());
    result.timeOfDayArchetype = archetype;
    if (dominant == 3)
        result.timeOfDayArchetype += " — Night Owl Gamer";

    char line[192];
    std::snprintf(line, sizeof(line), "Average session: %.0f minutes (%s)",
                  result.averageSessionSeconds / 60.0, result.gamingStyle.c_str());
    result.summaryLines.emplace_back(line);
    std::snprintf(line, sizeof(line), "%.0f%% Backlog Dust Score: %zu games launched once and forgotten",
                  result.backlogDustScore, result.abandonedGamesCount);
    result.summaryLines.emplace_back(line);
    result.summaryLines.push_back(result.timeOfDayArchetype);

    return result;
}

PlaytimeHabitsResult analyzePlaytimeHabitsFromLogs(const std::vector<GamePlayLog>& logs)
{
    return analyzePlaytimeHabits(buildSyntheticSessions(logs));
}

std::vector<PlaySession> buildSyntheticSessions(const std::vector<GamePlayLog>& logs)
{
    std::vector<PlaySession> sessions;
    sessions.reserve(logs.size());
    for (const auto& log : logs) {
        if (log.playtimeSeconds == 0 && log.lastPlayedUnix == 0)
            continue;
        PlaySession s;
        s.romPath = log.romPath;
        s.romName = log.romName;
        s.systemId = log.systemId;
        s.durationSeconds = log.playtimeSeconds > 0 ? log.playtimeSeconds : 60;
        s.timestampUnix = log.lastPlayedUnix;
        sessions.push_back(std::move(s));
    }
    return sessions;
}

} // namespace sf::analytics
