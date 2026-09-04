#include "util/LibraryCache.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

namespace sf {

namespace {

constexpr const char* kCacheFile = "library_index.json";

std::string cachePath()
{
    return FileSystem::join(paths::CACHE_DIR, kCacheFile);
}

Json metaToJson(const GameMetadata& m)
{
    Json j = Json::object();
    j["romPath"] = m.romPath;
    j["systemId"] = m.systemId;
    j["title"] = m.title;
    j["description"] = m.description;
    j["developer"] = m.developer;
    j["publisher"] = m.publisher;
    j["releaseDate"] = m.releaseDate;
    j["genre"] = m.genre;
    j["boxArtPath"] = m.boxArtPath;
    j["logoPath"] = m.logoPath;
    j["videoPath"] = m.videoPath;
    j["manualPath"] = m.manualPath;
    j["crc32"] = m.crc32;
    j["scraped"] = m.scraped;
    return j;
}

GameMetadata metaFromJson(const Json& j)
{
    GameMetadata m;
    if (!j.is_object())
        return m;
    m.romPath = j.value("romPath", std::string());
    m.systemId = j.value("systemId", std::string());
    m.title = j.value("title", std::string());
    m.description = j.value("description", std::string());
    m.developer = j.value("developer", std::string());
    m.publisher = j.value("publisher", std::string());
    m.releaseDate = j.value("releaseDate", std::string());
    m.genre = j.value("genre", std::string());
    m.boxArtPath = j.value("boxArtPath", std::string());
    m.logoPath = j.value("logoPath", std::string());
    m.videoPath = j.value("videoPath", std::string());
    m.manualPath = j.value("manualPath", std::string());
    m.crc32 = j.value("crc32", std::string());
    m.scraped = j.value("scraped", false);
    return m;
}

Json gameToJson(const GameItem& g)
{
    Json j = Json::object();
    j["path"] = g.path;
    j["filename"] = g.filename;
    j["displayName"] = g.displayName;
    j["systemId"] = g.systemId;
    j["meta"] = metaToJson(g.meta);
    return j;
}

GameItem gameFromJson(const Json& j)
{
    GameItem g;
    if (!j.is_object())
        return g;
    g.path = j.value("path", std::string());
    g.filename = j.value("filename", std::string());
    g.displayName = j.value("displayName", std::string());
    g.systemId = j.value("systemId", std::string());
    if (j.contains("meta"))
        g.meta = metaFromJson(j["meta"]);
    return g;
}

} // namespace

bool LibraryCache::save(const std::unordered_map<std::string, std::vector<GameItem>>& gamesBySystem)
{
    Json root = Json::object();
    root["version"] = static_cast<double>(kVersion);
    Json systems = Json::object();
    for (const auto& [systemId, games] : gamesBySystem) {
        Json arr = Json::array();
        for (const auto& g : games)
            arr.push_back(gameToJson(g));
        systems[systemId] = std::move(arr);
    }
    root["systems"] = std::move(systems);

    FileSystem::ensureAppDirectories();
    const std::string path = cachePath();
    if (!FileSystem::writeFile(path, root.dump())) {
        SF_LOG_W("Cache", "Failed to write library index");
        return false;
    }
    SF_LOG_I("Cache", "Saved library index (%zu systems)", gamesBySystem.size());
    return true;
}

bool LibraryCache::load(std::unordered_map<std::string, std::vector<GameItem>>& gamesBySystem)
{
    const std::string path = cachePath();
    if (!FileSystem::exists(path))
        return false;

    try {
        const std::string text = FileSystem::readFile(path);
        if (text.empty())
            return false;

        const Json root = Json::parse(text);
        if (!root.is_object())
            return false;
        if (static_cast<int>(root.value("version", 0.0)) != kVersion)
            return false;
        if (!root.contains("systems") || !root["systems"].is_object())
            return false;

        std::unordered_map<std::string, std::vector<GameItem>> loaded;
        for (const auto& [systemId, arr] : root["systems"].items()) {
            if (!arr.is_array())
                continue;
            std::vector<GameItem> games;
            games.reserve(arr.size());
            for (const auto& entry : arr) {
                GameItem g = gameFromJson(entry);
                if (!g.path.empty())
                    games.push_back(std::move(g));
            }
            if (!games.empty())
                loaded[systemId] = std::move(games);
        }

        if (loaded.empty())
            return false;

        gamesBySystem = std::move(loaded);
        SF_LOG_I("Cache", "Loaded library index (%zu systems)", gamesBySystem.size());
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Cache", "Library index parse failed: %s", ex.what());
        return false;
    }
}

} // namespace sf
