#include "util/Favorites.hpp"

#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <unordered_map>

namespace sf {

Favorites& Favorites::instance()
{
    static Favorites fav;
    return fav;
}

bool Favorites::load()
{
    bySystem_.clear();
    std::string data = FileSystem::readFile(paths::USER_FAVORITES_PATH);
    if (data.empty())
        return true;

    try {
        auto j = Json::parse(data);
        if (!j.contains("favorites") || !j["favorites"].is_object())
            return true;
        for (const auto& [systemId, val] : j["favorites"].items()) {
            if (!val.is_array())
                continue;
            for (const auto& entry : val) {
                if (entry.is_string())
                    bySystem_[systemId].push_back(entry.get<std::string>());
            }
        }
        SF_LOG_I("Favorites", "Loaded favorites for %zu systems", bySystem_.size());
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Favorites", "Parse error: %s", ex.what());
        return false;
    }
}

bool Favorites::save()
{
    Json root = Json::object();
    Json fav = Json::object();
    for (const auto& [systemId, paths] : bySystem_) {
        Json arr = Json::array();
        for (const auto& p : paths)
            arr.push_back(p);
        fav[systemId] = std::move(arr);
    }
    root["favorites"] = fav;

    if (!FileSystem::writeFile(paths::USER_FAVORITES_PATH, root.dump(2))) {
        SF_LOG_E("Favorites", "Failed to save %s", paths::USER_FAVORITES_PATH);
        return false;
    }
    return true;
}

bool Favorites::isFavorite(const std::string& systemId, const std::string& romPath) const
{
    auto it = bySystem_.find(systemId);
    if (it == bySystem_.end())
        return false;
    return std::find(it->second.begin(), it->second.end(), romPath) != it->second.end();
}

void Favorites::toggle(const std::string& systemId, const std::string& romPath)
{
    auto& list = bySystem_[systemId];
    auto it = std::find(list.begin(), list.end(), romPath);
    if (it != list.end()) {
        list.erase(it);
        SF_LOG_I("Favorites", "Removed favorite: %s", romPath.c_str());
    } else {
        list.insert(list.begin(), romPath);
        SF_LOG_I("Favorites", "Added favorite: %s", romPath.c_str());
    }
    save();
}

void Favorites::remove(const std::string& systemId, const std::string& romPath)
{
    auto it = bySystem_.find(systemId);
    if (it == bySystem_.end())
        return;
    auto& list = it->second;
    auto pos = std::find(list.begin(), list.end(), romPath);
    if (pos == list.end())
        return;
    list.erase(pos);
    save();
}

void Favorites::relocate(const std::string& systemId, const std::string& oldPath,
                         const std::string& newPath)
{
    if (oldPath == newPath)
        return;
    auto it = bySystem_.find(systemId);
    if (it == bySystem_.end())
        return;
    for (auto& path : it->second) {
        if (path == oldPath) {
            path = newPath;
            save();
            return;
        }
    }
}

void Favorites::sortGames(std::string systemId, std::vector<GameItem>& games) const
{
    auto it = bySystem_.find(systemId);
    if (it == bySystem_.end() || it->second.empty())
        return;

    const auto& order = it->second;
    std::stable_sort(games.begin(), games.end(), [&order](const GameItem& a, const GameItem& b) {
        auto rank = [&order](const std::string& path) -> int {
            auto pos = std::find(order.begin(), order.end(), path);
            if (pos == order.end())
                return static_cast<int>(order.size()) + 1;
            return static_cast<int>(std::distance(order.begin(), pos));
        };
        return rank(a.path) < rank(b.path);
    });
}

std::vector<std::pair<std::string, std::string>> Favorites::allPaths() const
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [systemId, paths] : bySystem_) {
        for (const auto& path : paths)
            out.emplace_back(systemId, path);
    }
    return out;
}

} // namespace sf
