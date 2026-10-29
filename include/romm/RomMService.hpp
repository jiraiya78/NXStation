#pragma once

#include "romm/RomMConfig.hpp"
#include "romm/RomMClient.hpp"
#include "romm/RomMSyncEngine.hpp"
#include "romm/RomMSaveMapper.hpp"
#include "romm/RomMStateStore.hpp"
#include "romm/RomMLibraryService.hpp"

#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <ctime>

namespace sf::romm {

/** RomM service - coordinates launch/exit sync and background work. */
class RomMService {
public:
    static RomMService& instance();

    RomMService(const RomMService&) = delete;
    RomMService& operator=(const RomMService&) = delete;

    /** Initialize the service with configuration. */
    bool initialize(const RomMConfig& config);

    /** Check if RomM sync is enabled and configured. */
    bool isEnabled() const { return enabled_; }

    /** Check if sync is currently in progress. */
    bool isSyncInProgress() const { return syncInProgress_.load(); }

    /** Get last sync result message. */
    std::string lastSyncResult() const { return lastSyncResult_; }

    /** Run pre-launch sync for a game (download saves/states). */
    bool syncBeforeLaunch(const std::string& romPath, const std::string& systemId,
                          const std::string& coreId,
                          std::function<void(const std::string&, int)> progress = {},
                          std::atomic<bool>* abort = nullptr);

    /** Run post-return sync on next startup (upload saves/states). */
    void syncAfterReturn(std::function<void(bool, const std::string&)> done);

    /** Run full sync for all ROMs (manual trigger from settings). */
    void syncAll(std::function<void(bool, const std::string&)> done,
                 std::function<void(const std::string&, int)> progress = {});

    /** Run sync for one ROM on a worker thread; done callback runs on the main thread. */
    void syncForRomAsync(const std::string& romPath, const std::string& systemId, const std::string& coreId,
                         std::function<void(bool, const std::string&)> done);

    /** True when this exact ROM was synced by this process very recently (avoids duplicate sync). */
    bool wasRecentlySynced(const std::string& romPath) const;

    /** Load the persisted library snapshot (call after initialize). */
    bool loadLibraryCache() {
        bool ok = library_->loadCache();
        rebuildMappingsFromCache();
        return ok;
    }

    /** Populate the "romm" virtual system in AppState from the library cache (main thread). */
    void populateVirtualLibrary();

    /** Fetch the library on a worker thread, persist the cache, then populate + refresh UI. */
    void refreshLibraryAsync(std::function<void(bool, const std::string&)> done);

    /** Download one library entry on a worker thread; registers the mapping + saves the cache. */
    void downloadRomAsync(const LibraryEntry& entry,
                          std::function<void(const std::string&, int)> progress,
                          std::atomic<bool>* abort,
                          std::function<void(bool, const std::string&)> done,
                          const std::vector<long long>& fileIds = {});

    /** Save mapping access for the download flow. */
    RomMSaveMapper& mapper() { return *mapper_; }

    /** Pair a new device using an 8-digit code. */
    bool pairDevice(const std::string& code, std::string& errorOut);

    /** Disconnect/revoke the current device. */
    bool disconnect(std::string& errorOut);

    /** Check if device is paired and registered. */
    bool isPaired();

    /** Get the current device ID. */
    std::string deviceId();

    /** Get the RomM client for direct API access. */
    RomMClient& client() { return client_; }

    /** Get the library service for ROM browsing/downloading. */
    RomMLibraryService& library() { return *library_; }

    /** Refresh internal config from Config singleton (call after settings change). */
    void refreshConfig();

private:
    RomMService();
    ~RomMService();

    RomMConfig config_;
    RomMClient client_;
    std::unique_ptr<RomMSaveMapper> mapper_;
    std::unique_ptr<RomMStateStore> store_;
    std::unique_ptr<RomMSyncEngine> syncEngine_;
    std::unique_ptr<RomMLibraryService> library_;

    std::atomic<bool> syncInProgress_{false};
    bool enabled_ = false;
    bool initialized_ = false;
    std::string lastSyncResult_;

    mutable std::mutex syncMutex_;
    std::string lastSyncedPath_;
    std::time_t lastSyncedAt_ = 0;

    void markSynced(const std::string& romPath);

    void applyConfig(const RomMConfig& config);
    bool ensureAuthenticated(std::string& errorOut);

    /** Re-register mappings for cache entries marked downloaded (recovers installs whose
        mapping file was overwritten by the library cache under the shared legacy name). */
    void rebuildMappingsFromCache();
};

} // namespace sf::romm
