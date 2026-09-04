#include "scraper/MetadataCache.hpp"

#include "app/Config.hpp"

#include "scraper/GamelistXml.hpp"

#include "util/FileSystem.hpp"

#include "util/Logger.hpp"

#include "util/Paths.hpp"



namespace sf {



namespace {



bool pathMissing(const std::string& path)

{

    return path.empty() || !FileSystem::exists(path);

}



std::string romRootFor(const std::string& systemId)

{

    if (const SystemConfig* sys = Config::instance().findSystem(systemId))

        return sys->path;

    return {};

}



} // namespace



MetadataCache::MetadataCache() = default;



std::string MetadataCache::metaPath(const std::string& systemId, const std::string& romStem) const

{

    return GamelistXml::gamelistPath(romRootFor(systemId));

}



std::string MetadataCache::artworkPath(const std::string& systemId, const std::string& romStem,

                                       const std::string& kind) const

{

    const std::string root = romRootFor(systemId);

    if (root.empty())

        return {};

    const std::string suffix = (kind == "thumb") ? "-thumb" : "-image";

    return FileSystem::join(FileSystem::join(root, "images"), romStem + suffix + ".jpg");

}



std::string MetadataCache::videoCachePath(const std::string& systemId, const std::string& romStem) const

{

    const std::string root = romRootFor(systemId);

    if (root.empty())

        return {};

    return FileSystem::join(FileSystem::join(root, "videos"), romStem + "-video.mp4");

}



void MetadataCache::applyLocalMedia(GameMetadata& meta, const std::string& systemId,

                                    const std::string& romStem) const

{

    const std::string root = romRootFor(systemId);

    if (root.empty())

        return;

    const std::string filename = FileSystem::filenameOf(meta.romPath);

    GamelistXml::applyFallbackMedia(meta, root, filename, romStem);

}



void MetadataCache::invalidateSystem(const std::string& systemId)

{

    const std::string prefix = systemId + "/";

    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = memory_.begin(); it != memory_.end();) {

        if (it->first.compare(0, prefix.size(), prefix) == 0)

            it = memory_.erase(it);

        else

            ++it;

    }

}



std::optional<GameMetadata> MetadataCache::load(const std::string& systemId, const std::string& romStem)

{

    const std::string key = systemId + "/" + romStem;

    {

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = memory_.find(key);

        if (it != memory_.end())

            return it->second;

    }

    return std::nullopt;

}



void MetadataCache::remember(const std::string& systemId, const std::string& romStem,

                             const GameMetadata& meta)

{

    const std::string key = systemId + "/" + romStem;

    std::lock_guard<std::mutex> lock(mutex_);

    memory_[key] = meta;

}



bool MetadataCache::save(const GameMetadata& meta)

{

    const std::string root = romRootFor(meta.systemId);

    if (root.empty())

        return false;



    const std::string romFilename = FileSystem::filenameOf(meta.romPath);

    if (!GamelistXml::saveEntry(root, meta, romFilename)) {

        SF_LOG_E("Meta", "Failed to write gamelist for %s", meta.romPath.c_str());

        return false;

    }



    const std::string stem = FileSystem::stemOf(meta.romPath);

    remember(meta.systemId, stem, meta);

    return true;

}



} // namespace sf

