#pragma once

#include "app/Models.hpp"
#include "util/ThreadPool.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <functional>

namespace sf {

class TextureCache;
class VideoPlayer;
class MetadataCache;
class ScraperService;

enum class MemoryMode {
    Title,   // Full title mode — all features enabled
    Applet,  // Restricted — disable video & high-res caches
};

using LibraryScanProgressCb =
    std::function<void(const std::string& systemName, const std::string& systemId, size_t current,
                       size_t total)>;

class AppState {
public:
    static AppState& instance();

    bool initialize();
    void shutdown();
    ~AppState();

    MemoryMode memoryMode() const { return memoryMode_; }
    bool isAppletMode() const { return memoryMode_ == MemoryMode::Applet; }
    bool videoAllowed() const;

    ThreadPool& pool() { return *pool_; }
    TextureCache& textures() { return *textures_; }
    VideoPlayer& video() { return *video_; }
    MetadataCache& metaCache() { return *metaCache_; }
    ScraperService& scraper() { return *scraper_; }

    /** Scan all configured systems. Safe from worker thread. */
    void scanAllSystems(bool force = false, LibraryScanProgressCb onProgress = nullptr);
    void scanSystem(const std::string& systemId, bool force = false,
                    LibraryScanProgressCb onProgress = nullptr);

    /** Block until a system's ROM list is ready (worker-safe). */
    void ensureSystemScanned(const std::string& systemId);
    bool isSystemScanned(const std::string& systemId) const;

    /** True when at least one system's ROM list is loaded in RAM. */
    bool libraryInMemory() const;

    /** Write the ROM index cache (survives NRO handoff relaunch). */
    void persistLibraryCache();

    /** @deprecated Background scan removed — use manual Scan Games or first-launch scan. */
    void startBackgroundLibraryScan();

    /** Rebuild Favorites / Last Played virtual libraries and gamelist.xml files. */
    void rebuildVirtualSections();

    const std::vector<GameItem>& gamesFor(const std::string& systemId) const;
    std::vector<std::string> systemIds() const;

    void setGames(const std::string& systemId, std::vector<GameItem> games);
    void updateGameMetadata(const std::string& systemId, const std::string& romPath,
                            const GameMetadata& meta);

    /** Remove a ROM from the in-memory index (and optionally from favorites / last played). */
    bool removeGame(const std::string& systemId, const std::string& romPath);
    /** Update path/filename/displayName after a successful on-disk rename. */
    bool renameGame(const std::string& systemId, const std::string& oldPath,
                    const std::string& newPath);

    const GameItem* findGame(const std::string& systemId, const std::string& romPath) const;

    /** True after beginHandoff() — background work must not touch the UI. */
    bool isHandoffPending() const { return handoffPending_; }

    /** Stop UI-facing workers before envSetNextLoad (safe to call from main thread). */
    void beginHandoff();

    /** @deprecated Use beginHandoff() */
    void prepareForLaunch() { beginHandoff(); }

private:
    AppState() = default;

    enum class SystemScanState { Pending, Scanning, Ready };

    std::atomic<bool> handoffPending_{false};

    MemoryMode memoryMode_ = MemoryMode::Title;
    std::unique_ptr<ThreadPool> pool_;
    std::unique_ptr<TextureCache> textures_;
    std::unique_ptr<VideoPlayer> video_;
    std::unique_ptr<MetadataCache> metaCache_;
    std::unique_ptr<ScraperService> scraper_;

    mutable std::mutex gamesMutex_;
    std::unordered_map<std::string, std::vector<GameItem>> gamesBySystem_;

    mutable std::mutex scanMutex_;
    std::condition_variable scanCv_;
    std::unordered_map<std::string, SystemScanState> scanState_;
};

} // namespace sf
