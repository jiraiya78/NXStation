#pragma once

#include "app/Models.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sf {

class Favorites {
public:
    static Favorites& instance();

    bool load();
    bool save();

    bool isFavorite(const std::string& systemId, const std::string& romPath) const;
    void toggle(const std::string& systemId, const std::string& romPath);
    void remove(const std::string& systemId, const std::string& romPath);
    void relocate(const std::string& systemId, const std::string& oldPath, const std::string& newPath);

    void sortGames(std::string systemId, std::vector<GameItem>& games) const;

    /** All starred ROM paths across systems (systemId, romPath). */
    std::vector<std::pair<std::string, std::string>> allPaths() const;

private:
    Favorites() = default;

    // systemId -> ordered list of rom paths (most recent favorite first)
    std::unordered_map<std::string, std::vector<std::string>> bySystem_;
};

} // namespace sf
