#include "analytics/PlaySessionTracker.hpp"

#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <ctime>

namespace sf::analytics {
namespace {

std::string lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool readPending(Json& out)
{
    if (!FileSystem::exists(paths::PLAYTIME_PENDING_PATH))
        return false;
    try {
        out = Json::parse(FileSystem::readFile(paths::PLAYTIME_PENDING_PATH));
        return out.is_object();
    } catch (...) {
        return false;
    }
}

Json loadAggregates()
{
    if (!FileSystem::exists(paths::PLAYTIME_NXSTATION_PATH))
        return Json::object();
    try {
        const Json root = Json::parse(FileSystem::readFile(paths::PLAYTIME_NXSTATION_PATH));
        return root.is_object() ? root : Json::object();
    } catch (...) {
        return Json::object();
    }
}

bool saveAggregates(const Json& root)
{
    FileSystem::createDirectories(paths::DATA_DIR);
    return FileSystem::writeFile(paths::PLAYTIME_NXSTATION_PATH, root.dump(2));
}

} // namespace

std::string PlaySessionTracker::normalizeKey(const std::string& romPath)
{
    return lower(romPath);
}

void PlaySessionTracker::beginSession(const std::string& systemId, const std::string& romPath,
                                      const std::string& romName, const std::string& coreName)
{
    if (romPath.empty())
        return;

    commitPendingSession();

    const std::string key = normalizeKey(romPath);
    Json root = loadAggregates();
    if (!root.contains("entries") || !root["entries"].is_object())
        root["entries"] = Json::object();

    Json& entries = root["entries"];
    Json entry = entries.contains(key) && entries[key].is_object() ? entries[key] : Json::object();
    entry["romPath"] = romPath;
    if (!romName.empty())
        entry["romName"] = romName;
    if (!systemId.empty())
        entry["systemId"] = systemId;
    if (!coreName.empty())
        entry["coreName"] = coreName;

    const uint64_t prevLaunches =
        entry.contains("launchCount") && entry["launchCount"].is_number()
            ? static_cast<uint64_t>(entry["launchCount"].get<double>())
            : 0;
    entry["launchCount"] = static_cast<double>(prevLaunches + 1);
    entries[key] = std::move(entry);
    saveAggregates(root);

    const std::time_t now = std::time(nullptr);
    Json pending = Json::object();
    pending["systemId"] = systemId;
    pending["romPath"] = romPath;
    pending["romName"] = romName.empty() ? FileSystem::stemOf(romPath) : romName;
    pending["coreName"] = coreName;
    pending["startUnix"] = static_cast<double>(now);

    FileSystem::createDirectories(paths::DATA_DIR);
    if (!FileSystem::writeFile(paths::PLAYTIME_PENDING_PATH, pending.dump())) {
        SF_LOG_W("Analytics", "Could not write play session pending file");
        return;
    }

    SF_LOG_I("Analytics", "Play session started: %s (%s)", romPath.c_str(), systemId.c_str());
}

void PlaySessionTracker::commitPendingSession()
{
    Json pending;
    if (!readPending(pending)) {
        FileSystem::removeFile(paths::PLAYTIME_PENDING_PATH);
        return;
    }

    FileSystem::removeFile(paths::PLAYTIME_PENDING_PATH);

    const std::string romPath = pending.value("romPath", std::string());
    if (romPath.empty())
        return;

    const std::time_t end = std::time(nullptr);
    const std::time_t start = static_cast<std::time_t>(pending.value("startUnix", 0.0));
    if (start <= 0 || end <= start) {
        SF_LOG_W("Analytics", "Ignored invalid play session pending for %s", romPath.c_str());
        return;
    }

    const uint64_t duration = static_cast<uint64_t>(end - start);
    if (duration < 5) {
        SF_LOG_I("Analytics", "Skipped short play session (%llu s) for %s",
                 static_cast<unsigned long long>(duration), romPath.c_str());
        return;
    }

    const std::string key = normalizeKey(romPath);
    Json root = loadAggregates();
    if (!root.contains("entries") || !root["entries"].is_object())
        root["entries"] = Json::object();

    Json& entries = root["entries"];
    Json entry = entries.contains(key) && entries[key].is_object() ? entries[key] : Json::object();

    entry["romPath"] = romPath;
    if (!pending.value("romName", std::string()).empty())
        entry["romName"] = pending["romName"];
    if (!pending.value("systemId", std::string()).empty())
        entry["systemId"] = pending["systemId"];
    if (!pending.value("coreName", std::string()).empty())
        entry["coreName"] = pending["coreName"];

    const uint64_t prevSeconds =
        entry.contains("playtimeSeconds") && entry["playtimeSeconds"].is_number()
            ? static_cast<uint64_t>(entry["playtimeSeconds"].get<double>())
            : 0;
    entry["playtimeSeconds"] = static_cast<double>(prevSeconds + duration);
    entry["lastPlayedUnix"] = static_cast<double>(end);

    entries[key] = std::move(entry);
    if (!saveAggregates(root)) {
        SF_LOG_W("Analytics", "Failed to save NXStation playtime aggregates");
        return;
    }

    SF_LOG_I("Analytics", "Play session +%llu s → %s (total %llu s)",
             static_cast<unsigned long long>(duration), romPath.c_str(),
             static_cast<unsigned long long>(prevSeconds + duration));
}

} // namespace sf::analytics
