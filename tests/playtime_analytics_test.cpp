#include "analytics/PersonalityMetrics.hpp"
#include "analytics/PlaytimeHabits.hpp"
#include "analytics/PlaytimeTypes.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++gFailures;
    }
}

sf::analytics::GamePlayLog makeLog(const char* name, const char* system, uint64_t seconds,
                                   const char* genre = "")
{
    sf::analytics::GamePlayLog g;
    g.romName = name;
    g.systemId = system;
    g.playtimeSeconds = seconds;
    g.lastPlayedUnix = 1'700'000'000ULL;
    g.genre = genre ? genre : "";
    g.releaseYear = 1991;
    return g;
}

void testEmptyPersonality()
{
    const auto r = sf::analytics::calculatePersonalityMetrics({});
    expect(!r.hasData, "empty personality hasData false");
    expect(!r.emptyReason.empty(), "empty personality reason");
}

void testSixteenBitPurist()
{
    std::vector<sf::analytics::GamePlayLog> logs;
    logs.push_back(makeLog("SMW", "snes", 10 * 3600));
    logs.push_back(makeLog("Sonic", "megadrive", 8 * 3600));
    logs.push_back(makeLog("Mario", "nes", 1 * 3600));
    const auto r = sf::analytics::calculatePersonalityMetrics(logs);
    expect(r.hasData, "16-bit has data");
    expect(r.primaryTag == "The 16-Bit Purist", "16-bit purist tag");
}

void testSerialSampler()
{
    std::vector<sf::analytics::GamePlayLog> logs;
    for (int i = 0; i < 55; ++i)
        logs.push_back(makeLog(("Game" + std::to_string(i)).c_str(), "nes", 120));
    const auto r = sf::analytics::calculatePersonalityMetrics(logs);
    expect(r.primaryTag == "The Serial Sampler", "serial sampler tag");
}

void testCompletionist()
{
    std::vector<sf::analytics::GamePlayLog> logs;
    logs.push_back(makeLog("FF6", "snes", 30 * 3600));
    logs.push_back(makeLog("Other", "nes", 2 * 3600));
    const auto r = sf::analytics::calculatePersonalityMetrics(logs);
    expect(r.primaryTag == "The Laser-Focused Completionist", "completionist tag");
}

void testHabitsEmpty()
{
    const auto r = sf::analytics::analyzePlaytimeHabits({});
    expect(!r.hasData, "empty habits");
}

void testHabitsAverageSession()
{
    std::vector<sf::analytics::PlaySession> sessions;
    sf::analytics::PlaySession a;
    a.durationSeconds = 20 * 60;
    a.timestampUnix = 1'700'000'000ULL;
    sessions.push_back(a);
    sf::analytics::PlaySession b = a;
    b.durationSeconds = 40 * 60;
    sessions.push_back(b);
    const auto r = sf::analytics::analyzePlaytimeHabits(sessions);
    expect(r.hasData, "habits has data");
    expect(std::fabs(r.averageSessionSeconds - 30.0 * 60.0) < 1.0, "avg session 30m");
    expect(r.gamingStyle.find("Casual") != std::string::npos, "casual style");
}

void testBacklogDust()
{
    const uint64_t old = 1'600'000'000ULL; // well over 30 days before 1.7B
    std::vector<sf::analytics::PlaySession> sessions;
    sf::analytics::PlaySession dust;
    dust.romName = "Forgotten";
    dust.romPath = "sdmc:/roms/nes/forgotten.nes";
    dust.durationSeconds = 5 * 60;
    dust.timestampUnix = old;
    sessions.push_back(dust);

    sf::analytics::PlaySession keeper;
    keeper.romName = "Keeper";
    keeper.romPath = "sdmc:/roms/nes/keeper.nes";
    keeper.durationSeconds = 2 * 3600;
    keeper.timestampUnix = 1'700'000'000ULL;
    sessions.push_back(keeper);

    const auto r = sf::analytics::analyzePlaytimeHabits(sessions);
    expect(r.abandonedGamesCount == 1, "one abandoned");
    expect(r.backlogDustScore > 40.f && r.backlogDustScore < 60.f, "50% dust score");
}

void testMidnightSession()
{
    // 2023-11-14 01:30:00 UTC — local hour depends on TZ; still exercises path.
    std::vector<sf::analytics::PlaySession> sessions;
    sf::analytics::PlaySession s;
    s.durationSeconds = 3600;
    s.timestampUnix = 1'699'934'200ULL;
    sessions.push_back(s);
    const auto r = sf::analytics::analyzePlaytimeHabits(sessions);
    expect(r.timeOfDay.size() == 4, "four day parts");
    expect(!r.timeOfDayArchetype.empty(), "archetype string");
}

} // namespace

int main()
{
    testEmptyPersonality();
    testSixteenBitPurist();
    testSerialSampler();
    testCompletionist();
    testHabitsEmpty();
    testHabitsAverageSession();
    testBacklogDust();
    testMidnightSession();

    if (gFailures == 0) {
        std::printf("All playtime analytics tests passed.\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "%d test(s) failed.\n", gFailures);
    return EXIT_FAILURE;
}
