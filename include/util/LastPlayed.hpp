#pragma once

#include "app/Models.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sf {

class LastPlayed {
public:
    struct Entry {
        std::string systemId;
        std::string romPath;
        uint64_t playedAt = 0; // unix epoch seconds (wall clock)
    };

    static LastPlayed& instance();

    bool load();
    bool save();

    void record(const std::string& systemId, const std::string& romPath);
    void remove(const std::string& systemId, const std::string& romPath);
    void relocate(const std::string& systemId, const std::string& oldPath, const std::string& newPath);

    /** Most recent first. */
    std::vector<std::pair<std::string, std::string>> orderedPaths() const;

    /** Most recent first, including playedAt timestamps. */
    const std::vector<Entry>& orderedEntries() const { return entries_; }

    /** Format unix seconds as "YYYY-MM-DD HH:MM". Empty if unknown/legacy. */
    static std::string formatPlayedAt(uint64_t playedAt);

private:
    LastPlayed() = default;

    std::vector<Entry> entries_;
    static constexpr size_t kMaxEntries = 200;
};

} // namespace sf
