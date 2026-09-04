#pragma once

#include "app/Models.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace sf {

class MetadataCache {
public:
    MetadataCache();

    std::optional<GameMetadata> load(const std::string& systemId, const std::string& romStem);
    void remember(const std::string& systemId, const std::string& romStem, const GameMetadata& meta);
    bool save(const GameMetadata& meta);

    std::string metaPath(const std::string& systemId, const std::string& romStem) const;
    std::string artworkPath(const std::string& systemId, const std::string& romStem, const std::string& kind) const;
    std::string videoCachePath(const std::string& systemId, const std::string& romStem) const;

    /** Fill boxArtPath / videoPath from standard on-disk locations when files exist. */
    void applyLocalMedia(GameMetadata& meta, const std::string& systemId, const std::string& romStem) const;

    /** Drop in-memory entries so the next load re-reads disk (e.g. after manual file drops). */
    void invalidateSystem(const std::string& systemId);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, GameMetadata> memory_;
};

} // namespace sf
