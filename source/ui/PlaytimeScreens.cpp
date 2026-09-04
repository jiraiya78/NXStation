#include "ui/PlaytimeScreens.hpp"

#include "analytics/PersonalityMetrics.hpp"
#include "analytics/PlayRankings.hpp"
#include "analytics/PlaytimeHabits.hpp"
#include "analytics/RetroArchPlaytime.hpp"
#include "ui/AnalyticsReportView.hpp"
#include "ui/MetricIcons.hpp"

#include <cstdio>
#include <sstream>

namespace sf::ui {

namespace {

std::string formatDuration(uint64_t seconds)
{
    const uint64_t h = seconds / 3600;
    const uint64_t m = (seconds % 3600) / 60;
    const uint64_t s = seconds % 60;
    char buf[64];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%lluh %llum", static_cast<unsigned long long>(h),
                      static_cast<unsigned long long>(m));
    else if (m > 0)
        std::snprintf(buf, sizeof(buf), "%llum %llus", static_cast<unsigned long long>(m),
                      static_cast<unsigned long long>(s));
    else
        std::snprintf(buf, sizeof(buf), "%llus", static_cast<unsigned long long>(s));
    return buf;
}

std::string formatRankingsGames(const std::vector<sf::analytics::RankedGame>& games, bool byLaunches)
{
    if (games.empty())
        return "No ranked games yet.";

    std::ostringstream out;
    int rank = 1;
    for (const auto& game : games) {
        out << rank++ << ". " << game.romName;
        if (!game.systemId.empty())
            out << " (" << game.systemId << ")";
        out << "\n   ";
        if (byLaunches)
            out << game.launchCount << " launch" << (game.launchCount == 1 ? "" : "es");
        else
            out << formatDuration(game.playtimeSeconds);
        if (byLaunches)
            out << " · " << formatDuration(game.playtimeSeconds) << " played";
        else if (game.launchCount > 0)
            out << " · " << game.launchCount << " launch" << (game.launchCount == 1 ? "" : "es");
        out << "\n";
    }
    return out.str();
}

std::string formatRankingsSystems(const std::vector<sf::analytics::RankedSystem>& systems,
                                  bool byLaunches)
{
    if (systems.empty())
        return "No ranked systems yet.";

    std::ostringstream out;
    int rank = 1;
    for (const auto& system : systems) {
        out << rank++ << ". " << system.systemId;
        out << " (" << system.uniqueGames << " game" << (system.uniqueGames == 1 ? "" : "s")
            << ")\n   ";
        if (byLaunches)
            out << system.launchCount << " launch" << (system.launchCount == 1 ? "" : "es");
        else
            out << formatDuration(system.playtimeSeconds);
        if (byLaunches)
            out << " · " << formatDuration(system.playtimeSeconds) << " played";
        else if (system.launchCount > 0)
            out << " · " << system.launchCount << " launch"
                << (system.launchCount == 1 ? "" : "es");
        out << "\n";
    }
    return out.str();
}

std::string heatmapSummary(const sf::analytics::PlaytimeHabitsResult& habits)
{
    std::ostringstream out;
    out << "365-day activity grid (intensity 0–4 per day):\n";
    if (habits.heatmap365.empty()) {
        out << "No dated sessions yet.";
        return out.str();
    }
    size_t shown = 0;
    for (const auto& day : habits.heatmap365) {
        if (day.intensity == 0)
            continue;
        out << day.date << "  level " << day.intensity << "  ("
            << (day.playSeconds / 60) << " min)\n";
        if (++shown >= 14) {
            out << "… " << (habits.heatmap365.size() - shown) << " more active days";
            break;
        }
    }
    if (shown == 0)
        out << "Sessions lack timestamps — enable RetroArch runtime logging.";
    return out.str();
}

AnalyticsSection section(const std::string& key, const std::string& title, const std::string& body)
{
    return AnalyticsSection{title, body, analyticsSectionIconRes(key)};
}

} // namespace

void presentPersonalityMetrics()
{
    const auto logs = sf::analytics::loadRetroArchPlayLogs();
    const auto metrics = sf::analytics::calculatePersonalityMetrics(logs);

    AnalyticsReportSpec spec;
    spec.windowTitle = "Personality Metrics";

    if (!metrics.hasData) {
        spec.heroTitle = "No data";
        spec.heroSubtitle = metrics.emptyReason;
        spec.heroIconRes = analyticsSectionIconRes("empty");
        spec.sections.push_back(section("note", "RetroArch data", metrics.emptyReason));
        AnalyticsReportView::present(std::move(spec));
        return;
    }

    spec.heroIconRes = personalityTagIconRes(metrics.primaryTag);
    spec.heroTitle = metrics.primaryTag;
    spec.heroSubtitle = metrics.tagDescription;

    {
        std::ostringstream body;
        body << "Total playtime: " << formatDuration(metrics.totalPlaytimeSeconds) << "\n";
        body << "Unique games tracked: " << metrics.uniqueGames << "\n\n";
        body << "Nostalgia epoch breakdown:\n";
        for (const auto& line : metrics.decadeLines)
            body << "  • " << line << "\n";
        spec.sections.push_back(section("overview", "Overview", body.str()));
    }

    {
        std::ostringstream body;
        for (const auto& line : metrics.timeWarpStats)
            body << "• " << line << "\n";
        spec.sections.push_back(section("timewarp", "Time-Warp Comparisons", body.str()));
    }

    spec.sections.push_back(section(
        "sources",
        "Data sources",
        "sdmc:/retroarch/playlists/*.lpl\n"
        "sdmc:/retroarch/playlists/logs/**/*.lrtl\n"
        "sdmc:/switch/NXStation/data/playtime_nxstation.json (NXStation launches)\n"
        "NXStation metadata (genre / release year) when available."));

    AnalyticsReportView::present(std::move(spec));
}

void presentPlaytimeAnalytics()
{
    const auto logs = sf::analytics::loadRetroArchPlayLogs();
    const auto habits = sf::analytics::analyzePlaytimeHabitsFromLogs(logs);

    AnalyticsReportSpec spec;
    spec.windowTitle = "Playtime Analytics";

    if (!habits.hasData) {
        spec.heroTitle = "No data";
        spec.heroSubtitle = habits.emptyReason;
        spec.heroIconRes = analyticsSectionIconRes("empty");
        spec.sections.push_back(section("note", "RetroArch data", habits.emptyReason));
        AnalyticsReportView::present(std::move(spec));
        return;
    }

    spec.heroIconRes = gamingStyleIconRes(habits.gamingStyle);
    spec.heroTitle = habits.gamingStyle;
    spec.heroSubtitle = "Session habits & backlog insights";

    {
        std::ostringstream body;
        body << "Total playtime: " << formatDuration(habits.totalPlaytimeSeconds) << "\n";
        body << "Synthetic sessions: " << habits.totalSessionCount
             << " (one per RetroArch title)\n";
        for (const auto& line : habits.summaryLines)
            body << "• " << line << "\n";
        spec.sections.push_back(section("session", "Session Style", body.str()));
    }

    {
        std::ostringstream body;
        for (const auto& slice : habits.timeOfDay)
            body << slice.label << ": " << static_cast<int>(slice.percent + 0.5f) << "%\n";
        body << "\n" << habits.timeOfDayArchetype;
        spec.sections.push_back(section("timeofday", "Time of Day", body.str()));
    }

    spec.sections.push_back(section("heatmap", "Activity Heatmap", heatmapSummary(habits)));

    const auto rankings = sf::analytics::buildPlayRankings(logs);
    spec.sections.push_back(section(
        "rank-time",
        "Top Games — Playtime",
        formatRankingsGames(rankings.topGamesByPlaytime, false)));
    spec.sections.push_back(section(
        "rank-launches",
        "Top Games — Launches",
        formatRankingsGames(rankings.topGamesByLaunches, true)));
    spec.sections.push_back(section(
        "rank-sys-time",
        "Top Systems — Playtime",
        formatRankingsSystems(rankings.topSystemsByPlaytime, false)));
    spec.sections.push_back(section(
        "rank-sys-launches",
        "Top Systems — Launches",
        formatRankingsSystems(rankings.topSystemsByLaunches, true)));

    if (!habits.abandonedGameNames.empty()) {
        std::ostringstream body;
        body << "Games played <10 min, last touched 30+ days ago:\n";
        for (const auto& name : habits.abandonedGameNames)
            body << "  • " << name << "\n";
        spec.sections.push_back(section("backlog", "Backlog Dust", body.str()));
    }

    spec.sections.push_back(section(
        "note",
        "Note",
        "RetroArch .lrtl logs store aggregate runtime per title, not per-session "
        "history. Heatmap and time-of-day use last-played timestamps.\n"
        "Launch counts come from NXStation game opens (playtime_nxstation.json)."));

    AnalyticsReportView::present(std::move(spec));
}

} // namespace sf::ui
