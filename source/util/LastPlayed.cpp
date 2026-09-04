#include "util/LastPlayed.hpp"

#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf {

namespace {

uint64_t nowSeconds()
{
#ifdef __SWITCH__
    // Prefer Switch user clock (POSIX UTC). newlib time() can be wrong/offset
    // depending on TZ init; time services match what the OS Settings show.
    u64 ts = 0;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &ts)) && ts > 0)
        return ts;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_NetworkSystemClock, &ts)) && ts > 0)
        return ts;
#endif
    const std::time_t t = std::time(nullptr);
    if (t <= 0)
        return 0;
    return static_cast<uint64_t>(t);
}

/** Legacy entries used CPU uptime seconds (~hundreds), not unix epoch. */
bool looksLikeUnixEpoch(uint64_t playedAt)
{
    return playedAt >= 1000000000ULL; // ~2001-09-09
}

} // namespace

LastPlayed& LastPlayed::instance()
{
    static LastPlayed history;
    return history;
}

std::string LastPlayed::formatPlayedAt(uint64_t playedAt)
{
    if (!looksLikeUnixEpoch(playedAt))
        return {};

#ifdef __SWITCH__
    // Convert with the console's own timezone rules (handles DST / region).
    TimeCalendarTime cal{};
    if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(playedAt, &cal, nullptr))) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u",
                      static_cast<unsigned>(cal.year), static_cast<unsigned>(cal.month),
                      static_cast<unsigned>(cal.day), static_cast<unsigned>(cal.hour),
                      static_cast<unsigned>(cal.minute));
        return buf;
    }
#endif

    const std::time_t t = static_cast<std::time_t>(playedAt);
    std::tm tm{};
#if defined(_WIN32)
    if (localtime_s(&tm, &t) != 0)
        return {};
#else
    if (!localtime_r(&t, &tm))
        return {};
#endif

    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0)
        return {};
    return buf;
}

bool LastPlayed::load()
{
    entries_.clear();
    const std::string data = FileSystem::readFile(paths::USER_LAST_PLAYED_PATH);
    if (data.empty())
        return true;

    try {
        const auto j = Json::parse(data);
        if (!j.contains("last_played") || !j["last_played"].is_array())
            return true;

        for (const auto& entry : j["last_played"]) {
            if (!entry.is_object())
                continue;
            Entry e;
            e.systemId = entry.value("systemId", std::string());
            e.romPath = entry.value("romPath", std::string());
            if (entry.contains("playedAt")) {
                const auto& at = entry["playedAt"];
                // Prefer string form (full integer digits). Number form is also accepted;
                // older dumps used 6-digit scientific notation and lost minute precision.
                if (at.is_string()) {
                    try {
                        e.playedAt = static_cast<uint64_t>(std::stoull(at.get<std::string>()));
                    } catch (...) {
                        e.playedAt = 0;
                    }
                } else if (at.is_number()) {
                    e.playedAt = static_cast<uint64_t>(at.get<double>());
                }
            }
            if (!e.systemId.empty() && !e.romPath.empty())
                entries_.push_back(std::move(e));
        }

        std::sort(entries_.begin(), entries_.end(),
                  [](const Entry& a, const Entry& b) { return a.playedAt > b.playedAt; });

        SF_LOG_I("LastPlayed", "Loaded %zu entries", entries_.size());
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("LastPlayed", "Parse error: %s", ex.what());
        return false;
    }
}

bool LastPlayed::save()
{
    Json root = Json::object();
    Json arr = Json::array();
    for (const auto& e : entries_) {
        Json item = Json::object();
        item["systemId"] = e.systemId;
        item["romPath"] = e.romPath;
        // Store as decimal string so a future JSON number-format bug cannot round times.
        item["playedAt"] = std::to_string(e.playedAt);
        arr.push_back(std::move(item));
    }
    root["last_played"] = std::move(arr);

    if (!FileSystem::writeFile(paths::USER_LAST_PLAYED_PATH, root.dump(2))) {
        SF_LOG_E("LastPlayed", "Failed to save %s", paths::USER_LAST_PLAYED_PATH);
        return false;
    }
    return true;
}

void LastPlayed::record(const std::string& systemId, const std::string& romPath)
{
    if (systemId.empty() || romPath.empty())
        return;

    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&](const Entry& e) { return e.systemId == systemId && e.romPath == romPath; }),
        entries_.end());

    entries_.insert(entries_.begin(), Entry{systemId, romPath, nowSeconds()});

    if (entries_.size() > kMaxEntries)
        entries_.resize(kMaxEntries);

    save();
    const uint64_t at = entries_.front().playedAt;
    SF_LOG_I("LastPlayed", "Recorded %s [%s] at %s (unix=%llu)", romPath.c_str(), systemId.c_str(),
             formatPlayedAt(at).c_str(), static_cast<unsigned long long>(at));
}

void LastPlayed::remove(const std::string& systemId, const std::string& romPath)
{
    const auto before = entries_.size();
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&](const Entry& e) { return e.systemId == systemId && e.romPath == romPath; }),
        entries_.end());
    if (entries_.size() != before)
        save();
}

void LastPlayed::relocate(const std::string& systemId, const std::string& oldPath,
                          const std::string& newPath)
{
    if (oldPath == newPath)
        return;
    bool changed = false;
    for (auto& e : entries_) {
        if (e.systemId == systemId && e.romPath == oldPath) {
            e.romPath = newPath;
            changed = true;
        }
    }
    if (changed)
        save();
}

std::vector<std::pair<std::string, std::string>> LastPlayed::orderedPaths() const
{
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_)
        out.emplace_back(e.systemId, e.romPath);
    return out;
}

} // namespace sf
