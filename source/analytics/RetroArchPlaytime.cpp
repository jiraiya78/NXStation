#include "analytics/RetroArchPlaytime.hpp"

#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <sstream>
#include <unordered_map>

namespace sf::analytics {
namespace {

struct RuntimeRecord {
    uint64_t seconds = 0;
    uint64_t lastPlayed = 0;
    std::string coreName;
    std::string contentKey;
};

std::string lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string basenameNoExt(const std::string& path)
{
    std::string name = FileSystem::filenameOf(path);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name.resize(dot);
    return name;
}

uint64_t parseRuntimeString(const std::string& runtime)
{
    if (runtime.empty())
        return 0;

    // Plain integer seconds fallback.
    bool digitsOnly = !runtime.empty();
    for (char c : runtime) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            digitsOnly = false;
            break;
        }
    }
    if (digitsOnly)
        return static_cast<uint64_t>(std::stoull(runtime));

    std::vector<int> parts;
    std::stringstream ss(runtime);
    std::string token;
    while (std::getline(ss, token, ':')) {
        try {
            parts.push_back(std::stoi(token));
        } catch (...) {
            return 0;
        }
    }
    if (parts.empty())
        return 0;
    if (parts.size() == 1)
        return static_cast<uint64_t>(parts[0]);
    if (parts.size() == 2)
        return static_cast<uint64_t>(parts[0] * 60 + parts[1]);
    return static_cast<uint64_t>((parts[0] * 3600) + (parts[1] * 60) + parts[2]);
}

uint64_t parseLastPlayed(const std::string& text)
{
    if (text.empty())
        return 0;

    std::tm tm{};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (text.size() >= 19 && text[4] == '-' && text[7] == '-'
        && std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute,
                       &second)
               == 6) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
    } else if (text.size() >= 10 && text[4] == '-' && text[7] == '-'
               && std::sscanf(text.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
    } else {
        return 0;
    }

    tm.tm_isdst = -1;
    const std::time_t t = std::mktime(&tm);
    if (t <= 0)
        return 0;
    return static_cast<uint64_t>(t);
}

std::string normalizeRomKey(const std::string& path)
{
    return lower(path);
}

std::string inferSystemIdFromDbName(const std::string& dbName)
{
    const std::string n = lower(dbName);
    if (n.find("nintendo - nes") != std::string::npos || n.find("famicom") != std::string::npos)
        return "nes";
    if (n.find("super nintendo") != std::string::npos || n.find("super famicom") != std::string::npos)
        return "snes";
    if (n.find("nintendo 64") != std::string::npos)
        return "n64";
    if (n.find("game boy advance") != std::string::npos)
        return "gba";
    if (n.find("game boy color") != std::string::npos)
        return "gbc";
    if (n.find("game boy") != std::string::npos)
        return "gb";
    if (n.find("nintendo ds") != std::string::npos)
        return "nds";
    if (n.find("3ds") != std::string::npos)
        return "3ds";
    if (n.find("mega drive") != std::string::npos || n.find("genesis") != std::string::npos)
        return "megadrive";
    if (n.find("master system") != std::string::npos)
        return "mastersystem";
    if (n.find("game gear") != std::string::npos)
        return "gamegear";
    if (n.find("saturn") != std::string::npos)
        return "saturn";
    if (n.find("dreamcast") != std::string::npos)
        return "dreamcast";
    if (n.find("playstation portable") != std::string::npos || n.find("psp") != std::string::npos)
        return "psp";
    if (n.find("playstation") != std::string::npos || n.find("psx") != std::string::npos)
        return "psx";
    if (n.find("pc engine") != std::string::npos || n.find("turbografx") != std::string::npos)
        return "pce";
    if (n.find("atari - 2600") != std::string::npos || n.find("atari 2600") != std::string::npos)
        return "atari2600";
    if (n.find("atari - 5200") != std::string::npos)
        return "atari5200";
    if (n.find("atari - 7800") != std::string::npos)
        return "atari7800";
    if (n.find("lynx") != std::string::npos)
        return "atarilynx";
    if (n.find("jaguar") != std::string::npos)
        return "atarijaguar";
    if (n.find("atari st") != std::string::npos)
        return "atarist";
    if (n.find("gamecube") != std::string::npos)
        return "gc";
    if (n.find("wii") != std::string::npos)
        return "wii";
    if (n.find("mame") != std::string::npos || n.find("fbneo") != std::string::npos
        || n.find("finalburn") != std::string::npos)
        return "arcade";
    if (n.find("neo geo") != std::string::npos)
        return "neogeo";
    if (n.find("cps1") != std::string::npos || n.find("capcom play system i") != std::string::npos)
        return "cps1";
    if (n.find("cps2") != std::string::npos || n.find("capcom play system ii") != std::string::npos)
        return "cps2";
    return {};
}

std::string inferSystemIdFromRomPath(const std::string& path)
{
    const std::string p = lower(path);
    const std::string marker = "/roms/";
    const size_t pos = p.find(marker);
    if (pos == std::string::npos)
        return {};
    const size_t start = pos + marker.size();
    const size_t end = p.find('/', start);
    if (end == std::string::npos || end <= start)
        return {};
    return p.substr(start, end - start);
}

std::string inferSystemIdFromCore(const std::string& core)
{
    const std::string c = lower(core);
    if (c.find("snes9x") != std::string::npos)
        return "snes";
    if (c.find("fceumm") != std::string::npos || c.find("nestopia") != std::string::npos)
        return "nes";
    if (c.find("mupen64") != std::string::npos || c.find("parallel_n64") != std::string::npos)
        return "n64";
    if (c.find("mgba") != std::string::npos || c.find("vba") != std::string::npos)
        return "gba";
    if (c.find("gambatte") != std::string::npos)
        return "gbc";
    if (c.find("genesis_plus") != std::string::npos || c.find("picodrive") != std::string::npos)
        return "megadrive";
    if (c.find("melonds") != std::string::npos)
        return "nds";
    if (c.find("pcsx") != std::string::npos || c.find("beetle_psx") != std::string::npos)
        return "psx";
    if (c.find("ppsspp") != std::string::npos)
        return "psp";
    if (c.find("flycast") != std::string::npos)
        return "dreamcast";
    if (c.find("yabause") != std::string::npos || c.find("kronos") != std::string::npos)
        return "saturn";
    if (c.find("dolphin") != std::string::npos)
        return "gc";
    if (c.find("fbneo") != std::string::npos || c.find("mame") != std::string::npos)
        return "arcade";
    if (c.find("stella") != std::string::npos)
        return "atari2600";
    if (c.find("mednafen_pce") != std::string::npos)
        return "pce";
    return {};
}

int defaultReleaseYearForSystem(const std::string& systemId)
{
    static const std::unordered_map<std::string, int> kYears = {
        {"atari2600", 1977}, {"atari5200", 1982}, {"atari7800", 1986}, {"atarist", 1985},
        {"atarilynx", 1989}, {"atarijaguar", 1993}, {"nes", 1985}, {"mastersystem", 1986},
        {"gb", 1989}, {"snes", 1991}, {"megadrive", 1989}, {"gamegear", 1991}, {"gbc", 1998},
        {"pce", 1987}, {"neogeo", 1990}, {"cps1", 1988}, {"cps2", 1993}, {"n64", 1996},
        {"psx", 1995}, {"saturn", 1995}, {"gba", 2001}, {"nds", 2004},
        {"dreamcast", 1999}, {"psp", 2005}, {"gc", 2001}, {"wii", 2006}, {"3ds", 2011},
        {"arcade", 1985},
    };
    const auto it = kYears.find(systemId);
    return it == kYears.end() ? 0 : it->second;
}

void enrichMetadata(GamePlayLog& log)
{
    if (log.systemId.empty())
        log.systemId = inferSystemIdFromRomPath(log.romPath);
    if (log.systemId.empty() && !log.coreName.empty())
        log.systemId = inferSystemIdFromCore(log.coreName);
    if (log.systemId.empty() && !log.systemName.empty())
        log.systemId = inferSystemIdFromDbName(log.systemName);

    if (log.releaseYear <= 0)
        log.releaseYear = defaultReleaseYearForSystem(log.systemId);

    if (!log.romPath.empty() && (log.genre.empty() || log.releaseYear <= 0)) {
        const std::string systemId = log.systemId;
        const std::string stem = FileSystem::stemOf(log.romPath);
        if (!systemId.empty() && !stem.empty()) {
            const std::string metaPath =
                FileSystem::join(paths::META_DIR, FileSystem::join(systemId, stem + ".json"));
            if (FileSystem::exists(metaPath)) {
                try {
                    const auto j = Json::parse(FileSystem::readFile(metaPath));
                    if (log.genre.empty() && j.contains("genre") && j["genre"].is_string())
                        log.genre = j["genre"].get<std::string>();
                    if (log.releaseYear <= 0 && j.contains("releaseDate")
                        && j["releaseDate"].is_string()) {
                        const std::string d = j["releaseDate"].get<std::string>();
                        if (d.size() >= 4) {
                            try {
                                log.releaseYear = std::stoi(d.substr(0, 4));
                            } catch (...) {
                            }
                        }
                    }
                } catch (...) {
                }
            }
        }
    }
}

void collectLrtlRecursive(const std::string& dir, const std::string& playlistsDir,
                          std::unordered_map<std::string, RuntimeRecord>& out)
{
    if (!FileSystem::isDirectory(dir))
        return;

    for (const auto& entry : FileSystem::listDirectory(dir)) {
        if (entry.isDirectory) {
            collectLrtlRecursive(entry.path, playlistsDir, out);
            continue;
        }
        if (!endsWith(lower(entry.name), ".lrtl"))
            continue;

        const std::string data = FileSystem::readFile(entry.path);
        if (data.empty())
            continue;

        try {
            const auto j = Json::parse(data);
            RuntimeRecord rec;
            if (j.contains("runtime") && j["runtime"].is_string())
                rec.seconds = parseRuntimeString(j["runtime"].get<std::string>());
            else if (j.contains("playtime_seconds") && j["playtime_seconds"].is_number())
                rec.seconds = static_cast<uint64_t>(j["playtime_seconds"].get<double>());
            if (j.contains("last_played") && j["last_played"].is_string())
                rec.lastPlayed = parseLastPlayed(j["last_played"].get<std::string>());

            std::string rel = entry.path;
            const std::string logsPrefix = FileSystem::join(playlistsDir, "logs");
            if (rel.rfind(logsPrefix, 0) == 0)
                rel = rel.substr(logsPrefix.size());
            while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\'))
                rel.erase(rel.begin());

            const std::string contentKey = basenameNoExt(entry.path);
            const size_t slash = rel.find_last_of("/\\");
            if (slash != std::string::npos && slash + 1 < rel.size())
                rec.coreName = rel.substr(0, slash);
            rec.contentKey = contentKey;

            const std::string mapKey = lower(contentKey) + "|" + lower(rec.coreName);
            auto& slot = out[mapKey];
            if (rec.seconds >= slot.seconds) {
                slot = rec;
                if (slot.lastPlayed == 0)
                    slot.lastPlayed = rec.lastPlayed;
            }
        } catch (const std::exception& ex) {
            SF_LOG_W("Analytics", "Bad runtime log %s: %s", entry.path.c_str(), ex.what());
        }
    }
}

void parsePlaylistFile(const std::string& path,
                       std::unordered_map<std::string, GamePlayLog>& byPath)
{
    const std::string data = FileSystem::readFile(path);
    if (data.empty())
        return;

    const std::string playlistLabel = basenameNoExt(path);

    try {
        const auto j = Json::parse(data);
        if (!j.contains("items") || !j["items"].is_array())
            return;

        for (const auto& item : j["items"]) {
            if (!item.is_object())
                continue;
            GamePlayLog log;
            log.romPath = item.value("path", std::string());
            log.romName = item.value("label", std::string());
            log.coreName = item.value("core_name", std::string());
            log.systemName = item.value("db_name", playlistLabel);
            if (log.romName.empty() && !log.romPath.empty())
                log.romName = FileSystem::stemOf(log.romPath);
            if (log.romPath.empty())
                continue;

            if (item.contains("runtime") && item["runtime"].is_string())
                log.playtimeSeconds = parseRuntimeString(item["runtime"].get<std::string>());
            if (item.contains("last_played") && item["last_played"].is_string())
                log.lastPlayedUnix = parseLastPlayed(item["last_played"].get<std::string>());

            enrichMetadata(log);
            byPath[normalizeRomKey(log.romPath)] = std::move(log);
        }
    } catch (const std::exception& ex) {
        SF_LOG_W("Analytics", "Bad playlist %s: %s", path.c_str(), ex.what());
    }
}

void mergeRuntime(std::unordered_map<std::string, GamePlayLog>& byPath,
                  const std::unordered_map<std::string, RuntimeRecord>& runtime)
{
    for (const auto& [key, rec] : runtime) {
        bool matched = false;
        for (auto& [pathKey, log] : byPath) {
            const std::string stem = lower(FileSystem::stemOf(log.romPath));
            const std::string content = lower(rec.contentKey);
            if (stem == content || stem.find(content) != std::string::npos
                || content.find(stem) != std::string::npos) {
                if (rec.seconds > log.playtimeSeconds)
                    log.playtimeSeconds = rec.seconds;
                if (rec.lastPlayed > log.lastPlayedUnix)
                    log.lastPlayedUnix = rec.lastPlayed;
                if (log.coreName.empty())
                    log.coreName = rec.coreName;
                matched = true;
            }
        }
        if (!matched) {
            GamePlayLog log;
            log.romName = rec.contentKey;
            log.coreName = rec.coreName;
            log.playtimeSeconds = rec.seconds;
            log.lastPlayedUnix = rec.lastPlayed;
            log.systemName = rec.coreName;
            enrichMetadata(log);
            const std::string synthetic = "lrtl:" + key;
            byPath[synthetic] = std::move(log);
        }
    }
}

void mergeNxStationAggregates(std::unordered_map<std::string, GamePlayLog>& byPath)
{
    if (!FileSystem::exists(paths::PLAYTIME_NXSTATION_PATH))
        return;

    try {
        const Json root = Json::parse(FileSystem::readFile(paths::PLAYTIME_NXSTATION_PATH));
        if (!root.contains("entries") || !root["entries"].is_object())
            return;

        for (const auto& [key, value] : root["entries"].items()) {
            if (!value.is_object())
                continue;

            GamePlayLog log;
            log.romPath = value.value("romPath", std::string());
            if (log.romPath.empty())
                log.romPath = key;
            log.romName = value.value("romName", FileSystem::stemOf(log.romPath));
            log.systemId = value.value("systemId", std::string());
            log.coreName = value.value("coreName", std::string());
            if (value.contains("playtimeSeconds") && value["playtimeSeconds"].is_number())
                log.playtimeSeconds = static_cast<uint64_t>(value["playtimeSeconds"].get<double>());
            if (value.contains("launchCount") && value["launchCount"].is_number())
                log.launchCount = static_cast<uint64_t>(value["launchCount"].get<double>());
            if (value.contains("lastPlayedUnix") && value["lastPlayedUnix"].is_number())
                log.lastPlayedUnix = static_cast<uint64_t>(value["lastPlayedUnix"].get<double>());

            enrichMetadata(log);
            const std::string mapKey = normalizeRomKey(log.romPath);
            auto it = byPath.find(mapKey);
            if (it == byPath.end()) {
                byPath[mapKey] = std::move(log);
                continue;
            }

            GamePlayLog& existing = it->second;
            existing.playtimeSeconds += log.playtimeSeconds;
            existing.launchCount += log.launchCount;
            if (log.lastPlayedUnix > existing.lastPlayedUnix)
                existing.lastPlayedUnix = log.lastPlayedUnix;
            if (existing.systemId.empty())
                existing.systemId = log.systemId;
            if (existing.coreName.empty())
                existing.coreName = log.coreName;
            if (existing.romName.empty())
                existing.romName = log.romName;
        }
    } catch (const std::exception& ex) {
        SF_LOG_W("Analytics", "Bad NXStation playtime file: %s", ex.what());
    }
}

} // namespace

std::vector<GamePlayLog> loadRetroArchPlayLogs(const std::string& retroArchDir)
{
    std::unordered_map<std::string, GamePlayLog> byPath;
    const std::string playlistsDir = FileSystem::join(retroArchDir, "playlists");

    if (FileSystem::isDirectory(playlistsDir)) {
        for (const auto& entry : FileSystem::listDirectory(playlistsDir)) {
            if (entry.isDirectory)
                continue;
            if (endsWith(lower(entry.name), ".lpl"))
                parsePlaylistFile(entry.path, byPath);
        }
    }

    const std::string legacyPaths[] = {
        FileSystem::join(retroArchDir, "content_history.lpl"),
        FileSystem::join(retroArchDir, "content_runtime.lpl"),
        FileSystem::join(playlistsDir, "content_history.lpl"),
        FileSystem::join(playlistsDir, "content_runtime.lpl"),
    };
    for (const auto& path : legacyPaths) {
        if (FileSystem::exists(path))
            parsePlaylistFile(path, byPath);
    }

    std::unordered_map<std::string, RuntimeRecord> runtime;
    collectLrtlRecursive(FileSystem::join(playlistsDir, "logs"), playlistsDir, runtime);
    mergeRuntime(byPath, runtime);
    mergeNxStationAggregates(byPath);

    std::vector<GamePlayLog> out;
    out.reserve(byPath.size());
    for (auto& [_, log] : byPath) {
        if (log.playtimeSeconds == 0 && log.lastPlayedUnix == 0 && log.launchCount == 0)
            continue;
        enrichMetadata(log);
        out.push_back(std::move(log));
    }

    std::sort(out.begin(), out.end(),
              [](const GamePlayLog& a, const GamePlayLog& b) {
                  return a.playtimeSeconds > b.playtimeSeconds;
              });

    SF_LOG_I("Analytics", "Loaded %zu RetroArch play log entries", out.size());
    return out;
}

} // namespace sf::analytics
