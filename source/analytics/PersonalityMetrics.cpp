#include "analytics/PersonalityMetrics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace sf::analytics {
namespace {

enum class Era { Gen70s, Gen80s, Gen90s, Gen2000s, Unknown };

enum class PlayBucket { SixteenBit, EightBit, Polygon, Arcade, Other };

uint64_t sumPlaytime(const std::vector<GamePlayLog>& logs)
{
    uint64_t total = 0;
    for (const auto& g : logs)
        total += g.playtimeSeconds;
    return total;
}

bool isArcadeSystem(const GamePlayLog& g)
{
    const auto id = g.systemId;
    if (id == "arcade" || id == "cps1" || id == "cps2")
        return true;
    std::string blob = g.systemName + " " + g.coreName + " " + g.romPath;
    for (char& c : blob)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return blob.find("mame") != std::string::npos || blob.find("fbneo") != std::string::npos
           || blob.find("finalburn") != std::string::npos
           || (blob.find("neo geo") != std::string::npos && blob.find("arcade") != std::string::npos);
}

PlayBucket classifyBucket(const GamePlayLog& g)
{
    if (isArcadeSystem(g))
        return PlayBucket::Arcade;

    const std::string id = g.systemId;
    if (id == "snes" || id == "megadrive" || id == "pce" || id == "gamegear" || id == "neogeo")
        return PlayBucket::SixteenBit;
    if (id == "nes" || id == "mastersystem" || id == "gb" || id == "gbc" || id == "atari7800"
        || id == "atari2600" || id == "atari5200")
        return PlayBucket::EightBit;
    if (id == "psx" || id == "n64" || id == "saturn" || id == "dreamcast")
        return PlayBucket::Polygon;
    return PlayBucket::Other;
}

Era classifyEra(const GamePlayLog& g)
{
    int year = g.releaseYear;
    if (year <= 0)
        year = 0;

    if (year >= 2000)
        return Era::Gen2000s;
    if (year >= 1990)
        return Era::Gen90s;
    if (year >= 1980)
        return Era::Gen80s;
    if (year >= 1970)
        return Era::Gen70s;

    const std::string id = g.systemId;
    if (id == "atari2600")
        return Era::Gen70s;
    if (id == "nes" || id == "mastersystem" || id == "atari7800" || id == "atari5200")
        return Era::Gen80s;
    if (id == "snes" || id == "megadrive" || id == "psx" || id == "n64" || id == "saturn" || id == "gb"
        || id == "gbc" || id == "pce" || id == "neogeo" || id == "cps1" || id == "cps2")
        return Era::Gen90s;
    if (id == "dreamcast" || id == "gba" || id == "psp" || id == "nds" || id == "gc" || id == "wii"
        || id == "3ds")
        return Era::Gen2000s;
    return Era::Unknown;
}

bool isJrpgGenre(const std::string& genre)
{
    std::string g = genre;
    for (char& c : g)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return g.find("rpg") != std::string::npos || g.find("role playing") != std::string::npos
           || g.find("role-playing") != std::string::npos || g.find("jrpg") != std::string::npos;
}

float bucketPercent(uint64_t bucketSeconds, uint64_t total)
{
    if (total == 0)
        return 0.f;
    return (static_cast<float>(bucketSeconds) / static_cast<float>(total)) * 100.f;
}

std::string formatPercentLine(const char* label, float pct)
{
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%.0f%% %s", pct, label);
    return buf;
}

} // namespace

PersonalityMetricsResult calculatePersonalityMetrics(const std::vector<GamePlayLog>& gameLogData)
{
    PersonalityMetricsResult result;

    std::vector<GamePlayLog> logs;
    logs.reserve(gameLogData.size());
    for (const auto& g : gameLogData) {
        if (g.playtimeSeconds > 0 || g.lastPlayedUnix > 0)
            logs.push_back(g);
    }

    if (logs.empty()) {
        result.emptyReason =
            "No playtime data yet. NXStation records sessions for games launched from "
            "the library. RetroArch menu launches also need Settings → Saving → "
            "Save runtime log enabled.";
        return result;
    }

    result.hasData = true;
    result.uniqueGames = logs.size();
    result.totalPlaytimeSeconds = sumPlaytime(logs);

    uint64_t sixteen = 0, eight = 0, polygon = 0, arcade = 0, jrpg = 0;
    uint64_t maxGame = 0;
    std::unordered_map<int, uint64_t> eraSeconds;

    for (const auto& g : logs) {
        maxGame = std::max(maxGame, g.playtimeSeconds);
        switch (classifyBucket(g)) {
            case PlayBucket::SixteenBit:
                sixteen += g.playtimeSeconds;
                break;
            case PlayBucket::EightBit:
                eight += g.playtimeSeconds;
                break;
            case PlayBucket::Polygon:
                polygon += g.playtimeSeconds;
                break;
            case PlayBucket::Arcade:
                arcade += g.playtimeSeconds;
                break;
            default:
                break;
        }
        if (isJrpgGenre(g.genre))
            jrpg += g.playtimeSeconds;

        const Era era = classifyEra(g);
        eraSeconds[static_cast<int>(era)] += g.playtimeSeconds;
    }

    const uint64_t total = result.totalPlaytimeSeconds;
    result.decadeBreakdown.gen70s = bucketPercent(eraSeconds[static_cast<int>(Era::Gen70s)], total);
    result.decadeBreakdown.gen80s = bucketPercent(eraSeconds[static_cast<int>(Era::Gen80s)], total);
    result.decadeBreakdown.gen90s = bucketPercent(eraSeconds[static_cast<int>(Era::Gen90s)], total);
    result.decadeBreakdown.gen2000s = bucketPercent(eraSeconds[static_cast<int>(Era::Gen2000s)], total);
    result.decadeBreakdown.unknown = bucketPercent(eraSeconds[static_cast<int>(Era::Unknown)], total);

    result.decadeLines.push_back(
        formatPercentLine("70s (2nd Gen — Atari 2600 era)", result.decadeBreakdown.gen70s));
    result.decadeLines.push_back(
        formatPercentLine("80s (3rd Gen — NES / Master System era)", result.decadeBreakdown.gen80s));
    result.decadeLines.push_back(formatPercentLine(
        "90s (4th–5th Gen — SNES / Genesis / PS1 / N64 era)", result.decadeBreakdown.gen90s));
    result.decadeLines.push_back(formatPercentLine(
        "2000s+ (6th Gen+ — Dreamcast / GBA / PSP / NDS era)", result.decadeBreakdown.gen2000s));
    if (result.decadeBreakdown.unknown > 0.5f)
        result.decadeLines.push_back(
            formatPercentLine("Unknown era", result.decadeBreakdown.unknown));

    const double marioClears = static_cast<double>(total) / 21600.0;
    const double flights = static_cast<double>(total) / 50400.0;
    const double quarters = (static_cast<double>(total) / 180.0) * 0.25;

    char buf[160];
    std::snprintf(buf, sizeof(buf), "Equivalent to %.1f mainline Mario clears (6h each).", marioClears);
    result.timeWarpStats.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "Equivalent to flying NYC → Tokyo %.1f times (14h each).", flights);
    result.timeWarpStats.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "You would have spent $%.2f in 90s arcade quarters!", quarters);
    result.timeWarpStats.emplace_back(buf);

    const float maxShare = bucketPercent(maxGame, total);
    const bool completionist =
        total >= 20ULL * 3600ULL && maxShare > 40.f && logs.size() >= 1;
    const bool sampler = logs.size() >= 50 && maxShare < 10.f;
    const float arcadePct = bucketPercent(arcade, total);
    const float sixteenPct = bucketPercent(sixteen, total);
    const float eightPct = bucketPercent(eight, total);
    const float polygonPct = bucketPercent(polygon, total);
    const float jrpgPct = bucketPercent(jrpg, total);

    std::unordered_set<int> generationsWithPlay;
    for (const auto& [era, sec] : eraSeconds) {
        if (sec > 0 && era != static_cast<int>(Era::Unknown))
            generationsWithPlay.insert(era);
    }

    if (completionist) {
        result.primaryTag = "The Laser-Focused Completionist";
        result.tagDescription =
            "One title dominates your library — deep mastery over variety.";
    } else if (sampler) {
        result.primaryTag = "The Serial Sampler";
        result.tagDescription =
            "You touch everything but commit to nothing — a curator of first impressions.";
    } else if (arcadePct > 50.f) {
        result.primaryTag = "The Arcade Junkie";
        result.tagDescription = "Coins, cabinets, and high-score chases define your retro life.";
    } else if (sixteenPct > 60.f) {
        result.primaryTag = "The 16-Bit Purist";
        result.tagDescription =
            "SNES, Mega Drive, and the golden 16-bit era own your playtime.";
    } else if (eightPct > 60.f) {
        result.primaryTag = "The 8-Bit Pioneer";
        result.tagDescription = "NES, Master System, and classic 8-bit vibes run the show.";
    } else if (polygonPct > 60.f) {
        result.primaryTag = "The Polygon Crusader";
        result.tagDescription = "32/64-bit 3D classics — PS1, N64, Saturn, Dreamcast.";
    } else if (jrpgPct > 50.f) {
        result.primaryTag = "The JRPG Scholar";
        result.tagDescription = "Story-driven RPG marathons are your natural habitat.";
    } else if (generationsWithPlay.size() >= 4) {
        result.primaryTag = "The Retro Renaissance Gamer";
        result.tagDescription =
            "Balanced curiosity across console generations — no single era owns you.";
    } else {
        result.primaryTag = "The Retro Renaissance Gamer";
        result.tagDescription =
            "Eclectic taste across systems — the default badge for well-rounded players.";
    }

    return result;
}

} // namespace sf::analytics
