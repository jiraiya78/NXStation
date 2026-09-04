#pragma once

#include "app/Models.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sf {

/** ES-DE compatible gamelist.xml under each system's ROM folder. */
class GamelistXml {
public:
    using EntryMap = std::unordered_map<std::string, GameMetadata>;

    static std::string gamelistPath(const std::string& romRoot);
    static EntryMap load(const std::string& romRoot, const std::string& systemId);

    /** Probe ES-DE images/ and videos/ folders when gamelist has no entry. */
    static void applyFallbackMedia(GameMetadata& meta, const std::string& romRoot,
                                   const std::string& romFilename, const std::string& romStem);

    /** Update or append a game entry and write gamelist.xml. */
    static bool saveEntry(const std::string& romRoot, const GameMetadata& meta,
                          const std::string& romFilename);

    /** Rewrite gamelist.xml from a full game list (virtual sections). */
    static bool saveList(const std::string& romRoot, const std::vector<GameItem>& games);

    static std::string resolvePath(const std::string& romRoot, const std::string& entry);
    static std::string romKeyFromPath(const std::string& pathField);
};

} // namespace sf
