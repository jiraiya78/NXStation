#pragma once

#include "app/Models.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sf {

/** Persists the in-memory ROM index so NRO handoff relaunches stay instant. */
class LibraryCache {
public:
    static constexpr int kVersion = 1;

    static bool save(const std::unordered_map<std::string, std::vector<GameItem>>& gamesBySystem);
    static bool load(std::unordered_map<std::string, std::vector<GameItem>>& gamesBySystem);
};

} // namespace sf
