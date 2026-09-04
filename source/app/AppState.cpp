#include "app/AppState.hpp"
#include "util/Network.hpp"
#include "app/Config.hpp"
#include "media/TextureCache.hpp"
#include "media/VideoPlayer.hpp"
#include "scraper/GamelistXml.hpp"
#include "scraper/MetadataCache.hpp"
#include "scraper/ScraperService.hpp"
#include "util/Favorites.hpp"
#include "util/FileSystem.hpp"
#include "util/LibraryCache.hpp"
#include "util/LastPlayed.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"
#include "util/Paths.hpp"
#include "util/RomTitle.hpp"
#include "util/Utf8.hpp"
#include "util/VirtualSystems.hpp"

#include "media/ThemeBgmPlayer.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/UiSfx.hpp"
#include "ui/ThemeManager.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <cctype>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf {

AppState& AppState::instance()
{
    static AppState state;
    return state;
}

AppState::~AppState()
{
    shutdown();
}

bool AppState::initialize()
{
    FileSystem::ensureAppDirectories();
    Config::instance().seedReferenceSettingsFiles();

    memoryMode_ = MemoryMode::Title;
#ifdef __SWITCH__
    AppletType type = appletGetAppletType();
    if (type == AppletType_LibraryApplet || type == AppletType_SystemApplet ||
        type == AppletType_OverlayApplet) {
        memoryMode_ = MemoryMode::Applet;
        SF_LOG_W("App", "Running in Applet Mode — video & high-res cache disabled");
    } else {
        SF_LOG_I("App", "Running in Title Mode");
    }
#endif

    // Prefer SD config, fall back to romfs
    if (!Config::instance().load(paths::CONFIG_PATH)) {
        if (!Config::instance().load(paths::CONFIG_FALLBACK)) {
            SF_LOG_E("App", "Failed to load roms_config.json");
            return false;
        }
    }

    size_t workers = isAppletMode() ? 1 : 3;
    pool_ = std::make_unique<ThreadPool>(workers);

    size_t texLimit = Config::instance().textureCacheLimit();
    if (isAppletMode())
        texLimit = std::min(texLimit, size_t{16});

    textures_ = std::make_unique<TextureCache>(texLimit);
    textures_->setHighResAllowed(!isAppletMode());

    video_ = std::make_unique<VideoPlayer>();
    video_->setEnabled(videoAllowed());
    video_->setHoverDelaySeconds(Config::instance().hoverDelaySeconds());
    video_->setAudioEnabled(videoAllowed() && Config::instance().videoAudioEnabled());
    video_->setAudioVolume(Config::instance().videoAudioVolume());
    video_->previewAudio().setEnabled(videoAllowed() && Config::instance().videoAudioEnabled());
    video_->previewAudio().setVolume(Config::instance().videoAudioVolume());

    sf::ui::setNavSoundEnabled(Config::instance().navSoundEnabled());
    sf::ui::setNavSoundVolume(Config::instance().navSoundVolume());
    audio::ThemeBgmPlayer::instance().setEnabled(Config::instance().bgmEnabled() && !isAppletMode());
    audio::ThemeBgmPlayer::instance().setVolume(Config::instance().bgmVolume());
    sf::ui::ThemeManager::instance().initialize(Config::instance().themeName());

    metaCache_ = std::make_unique<MetadataCache>();
    scraper_ = std::make_unique<ScraperService>();
    scraper_->setRateLimitMs(Config::instance().requestDelayMs());

    for (const auto& sys : Config::instance().systems())
        scanState_[sys.id] = SystemScanState::Pending;

    if (Config::instance().libraryScanCompleted()) {
        std::unordered_map<std::string, std::vector<GameItem>> cached;
        if (LibraryCache::load(cached)) {
            for (auto& [systemId, games] : cached) {
                setGames(systemId, std::move(games));
                scanState_[systemId] = SystemScanState::Ready;
            }
            SF_LOG_I("App", "Restored library from cache");
        }
    }

    SF_LOG_I("App", "Loading favorites / last played…");
    Favorites::instance().load();
    LastPlayed::instance().load();
    rebuildVirtualSections();

    return true;
}

void AppState::shutdown()
{
    beginHandoff();
    scraper_.reset();
    metaCache_.reset();
    video_.reset();
    textures_.reset();
    Network::shutdown();
    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
}

bool AppState::videoAllowed() const
{
    return !isAppletMode() && Config::instance().videoEnabled();
}

void AppState::scanAllSystems(bool force, LibraryScanProgressCb onProgress)
{
    for (const auto& sys : Config::instance().systems())
        scanSystem(sys.id, force, onProgress);
    rebuildVirtualSections();
}

bool AppState::isSystemScanned(const std::string& systemId) const
{
    std::lock_guard<std::mutex> lock(scanMutex_);
    const auto it = scanState_.find(systemId);
    return it != scanState_.end() && it->second == SystemScanState::Ready;
}

bool AppState::libraryInMemory() const
{
    std::lock_guard<std::mutex> lock(scanMutex_);
    for (const auto& [id, state] : scanState_) {
        if (state == SystemScanState::Ready)
            return true;
    }
    return false;
}

void AppState::persistLibraryCache()
{
    std::unordered_map<std::string, std::vector<GameItem>> snapshot;
    {
        std::lock_guard<std::mutex> lock(gamesMutex_);
        snapshot = gamesBySystem_;
    }
    LibraryCache::save(snapshot);
}

void AppState::ensureSystemScanned(const std::string& systemId)
{
    if (isVirtualSystemId(systemId))
        return;
    scanSystem(systemId, false);
}

void AppState::startBackgroundLibraryScan()
{
    // No-op — first-launch and manual Scan Games replace background scanning.
}

void AppState::scanSystem(const std::string& systemId, bool force,
                          LibraryScanProgressCb onProgress)
{
    const SystemConfig* sys = Config::instance().findSystem(systemId);
    if (!sys)
        return;

    {
        std::unique_lock<std::mutex> lock(scanMutex_);
        const auto it = scanState_.find(systemId);
        if (!force && it != scanState_.end()) {
            if (it->second == SystemScanState::Ready)
                return;
            if (it->second == SystemScanState::Scanning) {
                scanCv_.wait(lock, [this, &systemId] {
                    const auto state = scanState_.find(systemId);
                    return state == scanState_.end() ||
                           state->second != SystemScanState::Scanning;
                });
                return;
            }
        }
        scanState_[systemId] = SystemScanState::Scanning;
    }

    metaCache_->invalidateSystem(systemId);

    auto roms = FileSystem::scanRoms(sys->path, sys->extensions);
    const size_t romCount = roms.size();
    if (onProgress)
        onProgress(sys->name, systemId, 0, romCount);

    auto gamelist = GamelistXml::load(sys->path, systemId);
    std::vector<GameItem> games;
    games.reserve(roms.size());

    for (size_t ri = 0; ri < roms.size(); ++ri) {
        const auto& rom = roms[ri];
        GameItem g;
        g.path = rom.path;
        g.filename = rom.filename;
        g.displayName = RomTitle::fromStem(rom.stem);
        g.systemId = systemId;
        g.meta.romPath = rom.path;
        g.meta.systemId = systemId;
        g.meta.title = g.displayName;

        if (auto it = gamelist.find(rom.filename); it != gamelist.end()) {
            g.meta = it->second;
            g.meta.romPath = rom.path;
            g.meta.systemId = systemId;
            g.meta.title = Utf8::ensureUtf8(g.meta.title);
            g.meta.description = Utf8::ensureUtf8(g.meta.description);
            if (!g.meta.title.empty())
                g.displayName = RomTitle::fromStem(g.meta.title);
            metaCache_->remember(systemId, rom.stem, g.meta);
        }
        // Local ES-DE media paths are resolved on focus — avoids thousands of SD
        // stat() calls during bulk scans of large libraries.
        games.push_back(std::move(g));
        if (onProgress)
            onProgress(sys->name, systemId, ri + 1, romCount);
    }

    std::stable_sort(games.begin(), games.end(),
                     [](const GameItem& a, const GameItem& b) {
                         return Utf8::compareTitles(a.displayName, b.displayName);
                     });

    SF_LOG_I("App", "Scanned %s -> %zu games", systemId.c_str(), games.size());
    setGames(systemId, std::move(games));
    persistLibraryCache();

    {
        std::lock_guard<std::mutex> lock(scanMutex_);
        scanState_[systemId] = SystemScanState::Ready;
    }
    scanCv_.notify_all();
}

const std::vector<GameItem>& AppState::gamesFor(const std::string& systemId) const
{
    static const std::vector<GameItem> empty;
    std::lock_guard<std::mutex> lock(gamesMutex_);
    auto it = gamesBySystem_.find(systemId);
    if (it == gamesBySystem_.end())
        return empty;
    return it->second;
}

std::vector<std::string> AppState::systemIds() const
{
    std::vector<std::string> ids;
    for (const auto& s : Config::instance().systems())
        ids.push_back(s.id);
    return ids;
}

void AppState::setGames(const std::string& systemId, std::vector<GameItem> games)
{
    std::lock_guard<std::mutex> lock(gamesMutex_);
    gamesBySystem_[systemId] = std::move(games);
}

void AppState::updateGameMetadata(const std::string& systemId, const std::string& romPath,
                                    const GameMetadata& meta)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(gamesMutex_);
        auto it = gamesBySystem_.find(systemId);
        if (it == gamesBySystem_.end())
            return;
        for (auto& g : it->second) {
            if (g.path != romPath)
                continue;
            g.meta = meta;
            if (!meta.title.empty()) {
                g.displayName = RomTitle::fromStem(meta.title);
                changed = true;
            }
            break;
        }

        // Keep Last Played in recency order; only A–Z sort normal systems / Favorites.
        if (changed && systemId != kLastPlayedSystemId) {
            std::stable_sort(it->second.begin(), it->second.end(),
                             [](const GameItem& a, const GameItem& b) {
                                 return Utf8::compareTitles(a.displayName, b.displayName);
                             });
        }
    }
    persistLibraryCache();
}

bool AppState::removeGame(const std::string& systemId, const std::string& romPath)
{
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(gamesMutex_);
        auto it = gamesBySystem_.find(systemId);
        if (it == gamesBySystem_.end())
            return false;
        auto& games = it->second;
        const auto before = games.size();
        games.erase(std::remove_if(games.begin(), games.end(),
                                   [&](const GameItem& g) { return g.path == romPath; }),
                    games.end());
        removed = games.size() != before;
    }
    if (!removed)
        return false;

    Favorites::instance().remove(systemId, romPath);
    LastPlayed::instance().remove(systemId, romPath);
    rebuildVirtualSections();
    persistLibraryCache();
    return true;
}

bool AppState::renameGame(const std::string& systemId, const std::string& oldPath,
                          const std::string& newPath)
{
    if (oldPath == newPath)
        return false;

    bool renamed = false;
    {
        std::lock_guard<std::mutex> lock(gamesMutex_);
        auto it = gamesBySystem_.find(systemId);
        if (it == gamesBySystem_.end())
            return false;
        for (auto& g : it->second) {
            if (g.path != oldPath)
                continue;
            g.path = newPath;
            g.filename = FileSystem::filenameOf(newPath);
            g.displayName = RomTitle::fromStem(FileSystem::stemOf(newPath));
            g.meta.romPath = newPath;
            renamed = true;
            break;
        }
        if (renamed && systemId != kLastPlayedSystemId) {
            std::stable_sort(it->second.begin(), it->second.end(),
                             [](const GameItem& a, const GameItem& b) {
                                 return Utf8::compareTitles(a.displayName, b.displayName);
                             });
        }
    }
    if (!renamed)
        return false;

    Favorites::instance().relocate(systemId, oldPath, newPath);
    LastPlayed::instance().relocate(systemId, oldPath, newPath);
    rebuildVirtualSections();
    persistLibraryCache();
    return true;
}

const GameItem* AppState::findGame(const std::string& systemId, const std::string& romPath) const
{
    std::lock_guard<std::mutex> lock(gamesMutex_);
    auto it = gamesBySystem_.find(systemId);
    if (it == gamesBySystem_.end())
        return nullptr;
    for (const auto& g : it->second) {
        if (g.path == romPath)
            return &g;
    }
    return nullptr;
}

void AppState::rebuildVirtualSections()
{
    auto resolve = [this](const std::string& systemId, const std::string& romPath) -> GameItem {
        if (const GameItem* found = findGame(systemId, romPath))
            return *found;

        GameItem g;
        g.path = romPath;
        g.filename = FileSystem::filenameOf(romPath);
        g.displayName = RomTitle::fromStem(FileSystem::stemOf(romPath));
        g.systemId = systemId;
        g.meta.romPath = romPath;
        g.meta.systemId = systemId;
        g.meta.title = g.displayName;
        metaCache_->applyLocalMedia(g.meta, systemId, FileSystem::stemOf(romPath));
        return g;
    };

    std::vector<GameItem> favorites;
    for (const auto& [systemId, romPath] : Favorites::instance().allPaths()) {
        if (!FileSystem::exists(romPath))
            continue;
        favorites.push_back(resolve(systemId, romPath));
    }

    std::stable_sort(favorites.begin(), favorites.end(),
                     [](const GameItem& a, const GameItem& b) {
                         return Utf8::compareTitles(a.displayName, b.displayName);
                     });

    std::vector<GameItem> recent;
    for (const auto& entry : LastPlayed::instance().orderedEntries()) {
        if (!FileSystem::exists(entry.romPath))
            continue;
        GameItem g = resolve(entry.systemId, entry.romPath);
        g.lastPlayedAt = entry.playedAt;
        recent.push_back(std::move(g));
    }

    GamelistXml::saveList(paths::FAVORITES_SECTION_DIR, favorites);
    GamelistXml::saveList(paths::LAST_PLAYED_SECTION_DIR, recent);

    setGames(kFavoritesSystemId, std::move(favorites));
    setGames(kLastPlayedSystemId, std::move(recent));

    SF_LOG_I("App", "Virtual sections: %zu favorites, %zu last played",
             gamesFor(kFavoritesSystemId).size(), gamesFor(kLastPlayedSystemId).size());
}

void AppState::beginHandoff()
{
    if (handoffPending_.exchange(true))
        return;

    SF_LOG_I("App", "Preparing for NRO handoff — flushing media & workers");
    NavigationState::persistForHandoff();
    if (scraper_) {
        scraper_->requestAbort();
        scraper_->cancelInflight();
    }
    if (video_)
        video_->shutdown();
    audio::ThemeBgmPlayer::instance().shutdown();
    if (textures_)
        textures_->flush();
    if (pool_)
        pool_->clear();
}

} // namespace sf
