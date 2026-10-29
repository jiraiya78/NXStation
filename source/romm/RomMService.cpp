#include "romm/RomMService.hpp"
#include "romm/RomMConfig.hpp"
#include "romm/RomMClient.hpp"
#include "romm/RomMSyncEngine.hpp"
#include "romm/RomMSaveMapper.hpp"
#include "romm/RomMStateStore.hpp"
#include "romm/RomMLibraryService.hpp"
#include "app/Config.hpp"
#include "app/AppState.hpp"
#include "scraper/MetadataCache.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"
#include "util/RomTitle.hpp"
#include "util/Utf8.hpp"
#include "util/VirtualSystems.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <borealis.hpp>

namespace sf::romm {

namespace {

std::string formatSize(int64_t bytes)
{
    char buf[32];
    if (bytes >= 1024LL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return buf;
}

} // namespace

RomMService& RomMService::instance() {
    static RomMService service;
    return service;
}

RomMService::RomMService() = default;

RomMService::~RomMService() = default;

bool RomMService::initialize(const RomMConfig& config) {
    config_ = config;
    applyConfig(config);

    // Load persisted state
    store_->load();

    // Restore tokens from state store
    if (!store_->accessToken().empty()) {
        client_.configure(config_.serverUrl, store_->accessToken(), store_->clientToken());
    }

    enabled_ = config_.enabled && !config_.serverUrl.empty();
    initialized_ = true;

    SF_LOG_I("RomM", "Service initialized: enabled=%d, server=%s, device=%s",
             enabled_, config_.serverUrl.c_str(), config_.deviceId.c_str());
    return true;
}

void RomMService::applyConfig(const RomMConfig& config) {
    config_ = config;
    mapper_ = std::make_unique<RomMSaveMapper>(config_);
    store_ = std::make_unique<RomMStateStore>(config_);
    syncEngine_ = std::make_unique<RomMSyncEngine>(client_, *mapper_, *store_, config_);
    library_ = std::make_unique<RomMLibraryService>(client_, config_);
}

void RomMService::refreshConfig() {
    const RomMConfig& latestConfig = sf::Config::instance().rommConfig();
    // Only update mutable fields that can change at runtime
    config_.enabled = latestConfig.enabled;
    config_.serverUrl = latestConfig.serverUrl;
    config_.deviceName = latestConfig.deviceName;
    config_.syncSaves = latestConfig.syncSaves;
    config_.syncStates = latestConfig.syncStates;
    config_.syncBeforeLaunch = latestConfig.syncBeforeLaunch;
    config_.syncAfterReturn = latestConfig.syncAfterReturn;
    config_.conflictPolicy = latestConfig.conflictPolicy;
    config_.stateCoreMatching = latestConfig.stateCoreMatching;
    config_.romMappingFile = latestConfig.romMappingFile;
    
    // Update enabled state
    enabled_ = config_.enabled && !config_.serverUrl.empty();
    
    SF_LOG_I("RomM", "Config refreshed: enabled=%d, server=%s", enabled_, config_.serverUrl.c_str());
}

void RomMService::rebuildMappingsFromCache() {
    if (!mapper_ || !library_) return;

    // The library cache records which entries were downloaded and where. Installs whose
    // mapping data was overwritten by the cache (shared legacy filename) recover their
    // mappings from these entries, so "Sync Now" still has work after a restart.
    size_t rebuilt = 0;
    for (const auto& entry : library_->entries()) {
        if (entry.rom.id.empty() || entry.localPath.empty() || !entry.downloadedLocally)
            continue;
        if (mapper_->getMappingByRomMId(entry.rom.id))
            continue;

        const SystemConfig* sys = sf::Config::instance().findSystem(entry.matchedSystemId);
        mapper_->addOrUpdateMapping(entry.rom.id, entry.matchedSystemId, entry.localPath,
                                    entry.rom.name, sys ? sys->core : "");
        ++rebuilt;
    }

    if (rebuilt > 0)
        SF_LOG_I("RomM", "Rebuilt %zu ROM mapping(s) from the library cache", rebuilt);
}

bool RomMService::ensureAuthenticated(std::string& errorOut) {
    if (!client_.isReady()) {
        errorOut = "RomM not configured or authenticated";
        return false;
    }

    // Check token expiration
    if (store_->tokenExpiresAt() > 0 && store_->tokenExpiresAt() < std::time(nullptr) + 60) {
        // Token expired or expiring soon - would need refresh logic
        // For now, just warn
        SF_LOG_W("RomM", "Access token may be expired");
    }

    return true;
}

bool RomMService::syncBeforeLaunch(const std::string& romPath, const std::string& systemId,
                                   const std::string& coreId,
                                   std::function<void(const std::string&, int)> progress,
                                   std::atomic<bool>* abort) {
    refreshConfig();
    if (!enabled_) return true; // Not an error, just skip

    std::string error;
    if (!ensureAuthenticated(error)) {
        lastSyncResult_ = "Auth error: " + error;
        if (progress) progress(lastSyncResult_, 0);
        return false;
    }

    // Wrap progress callback
    auto detailedProgress = [progress](const SyncDetailedProgress& p) {
        if (progress) progress(p.message, p.percent);
    };

    bool result = syncEngine_->syncForRom(romPath, systemId, coreId, detailedProgress, abort);
    lastSyncResult_ = syncEngine_->lastResult();
    if (result) markSynced(romPath);
    return result;
}

void RomMService::syncAfterReturn(std::function<void(bool, const std::string&)> done) {
    refreshConfig();
    if (!enabled_ || !config_.syncAfterReturn) {
        if (done) done(true, "RomM sync disabled or not configured");
        return;
    }

    if (syncInProgress_.exchange(true)) {
        if (done) done(false, "Sync already in progress");
        return;
    }

    std::string error;
    if (!ensureAuthenticated(error)) {
        syncInProgress_.store(false);
        if (done) done(false, "Auth error: " + error);
        return;
    }

    // Run sync on worker thread
    AppState::instance().pool().enqueue([this, done = std::move(done)]() mutable {
        auto progress = [](const SyncDetailedProgress&) {};
        std::atomic<bool> abort{false};

        bool result = syncEngine_->syncAll(progress, &abort);
        std::string message = syncEngine_->lastResult();
        lastSyncResult_ = message;

        syncInProgress_.store(false);

        if (done) {
            brls::sync([done = std::move(done), result, message]() mutable {
                done(result, message);
            });
        }
    });
}

void RomMService::syncForRomAsync(const std::string& romPath, const std::string& systemId,
                                  const std::string& coreId,
                                  std::function<void(bool, const std::string&)> done) {
    refreshConfig();
    if (!enabled_) {
        if (done) done(true, "RomM sync disabled");
        return;
    }

    std::string error;
    if (!ensureAuthenticated(error)) {
        if (done) done(false, "Auth error: " + error);
        return;
    }

    AppState::instance().pool().enqueue([this, romPath, systemId, coreId, done = std::move(done)]() mutable {
        auto progress = [](const SyncDetailedProgress&) {};
        std::atomic<bool> abort{false};

        bool result = syncEngine_->syncForRom(romPath, systemId, coreId, progress, &abort);
        std::string message = syncEngine_->lastResult();
        lastSyncResult_ = message;
        if (result) markSynced(romPath);

        if (done) {
            brls::sync([done = std::move(done), result, message]() mutable {
                done(result, message);
            });
        }
    });
}

bool RomMService::wasRecentlySynced(const std::string& romPath) const {
    std::lock_guard<std::mutex> lock(syncMutex_);
    return lastSyncedPath_ == romPath && lastSyncedAt_ > 0 &&
           std::time(nullptr) - lastSyncedAt_ < 120;
}

void RomMService::markSynced(const std::string& romPath) {
    std::lock_guard<std::mutex> lock(syncMutex_);
    lastSyncedPath_ = romPath;
    lastSyncedAt_ = std::time(nullptr);
}

void RomMService::populateVirtualLibrary() {
    std::vector<GameItem> items;
    items.reserve(library_->entries().size());

    // Cloud-only rows are also distributed into the real system sections, so a RomM game
    // shows up under its own system with the cloud badge (A downloads, A again plays).
    std::map<std::string, std::vector<GameItem>> cloudBySystem;
    size_t insertedRows = 0;

    for (const auto& entry : library_->entries()) {
        GameItem g;
        g.fromRomM = true;
        g.rommRomId = entry.rom.id;
        g.systemId = entry.matchedSystemId;
        g.path = entry.localPath;
        g.filename = entry.rom.files.empty() ? entry.rom.name : entry.rom.files.front().fileName;
        g.displayName = entry.rom.name.empty()
                            ? RomTitle::fromStem(FileSystem::stemOf(g.filename))
                            : entry.rom.name;
        g.meta.romPath = entry.localPath;
        g.meta.systemId = entry.matchedSystemId;
        g.meta.title = g.displayName;
        g.discCount = static_cast<int>(entry.rom.files.size());
        g.cloudAvailable = !entry.downloadedLocally;

        // RomM has already resolved this metadata, so cloud rows show a description and the
        // usual details even when they were never scraped locally.
        if (g.cloudAvailable) {
            g.meta.description = entry.rom.summary;
            g.meta.genre = entry.rom.genres;
            g.meta.developer = entry.rom.developer;
            g.meta.publisher = entry.rom.publisher;
            g.meta.releaseDate = entry.rom.releaseDate;
        }

        // Local scrape cache hits for already-downloaded titles still show art / metadata.
        if (entry.downloadedLocally) {
            AppState::instance().metaCache().applyLocalMedia(g.meta, entry.matchedSystemId,
                                                             FileSystem::stemOf(entry.localPath));
        }

        if (g.cloudAvailable && !g.systemId.empty()) {
            cloudBySystem[g.systemId].push_back(g);
        } else if (!g.systemId.empty() && sf::Config::instance().findSystem(g.systemId)) {
            // Already-downloaded games are no longer in cloudBySystem, so mergeCloudGames
            // cannot bring an old cloud-only row up to date; update the row directly so the
            // system section shows "Downloaded" instead of the cloud badge right after a download.
            // A game downloaded after the last scan has no row in the persisted index cache,
            // so refreshDownloadedRow inserts it; without that it would survive only in Last
            // Played after a restart.
            if (!AppState::instance().refreshDownloadedRow(g.systemId, g))
                ++insertedRows;
        }

        items.push_back(std::move(g));
    }

    const auto byTitle = [](const GameItem& a, const GameItem& b) {
        return Utf8::compareTitles(a.displayName, b.displayName);
    };

    for (auto& [systemId, cloudGames] : cloudBySystem) {
        std::stable_sort(cloudGames.begin(), cloudGames.end(), byTitle);
        AppState::instance().setCloudGames(systemId, cloudGames);
        AppState::instance().mergeCloudGames(systemId);
    }

    // Remove duplicate rows for the same file (a scanned row and a RomM row can coexist
    // when the RomM id was lost from the persisted cache), so lists do not grow on every
    // startup.
    size_t deduped = 0;
    for (const auto& sys : sf::Config::instance().systems())
        deduped += AppState::instance().dedupeSystemGames(sys.id);

    std::stable_sort(items.begin(), items.end(), byTitle);

    const size_t total = items.size();
    const size_t downloaded = static_cast<size_t>(
        std::count_if(items.begin(), items.end(), [](const GameItem& g) { return !g.path.empty(); }));

    AppState::instance().setGames(kRomMSystemId, std::move(items));
    AppState::instance().rebuildVirtualSections();

    if (insertedRows > 0 || deduped > 0) {
        // Persist rows that were inserted or cleaned up so the sections stay stable across
        // restarts without a full rescan.
        AppState::instance().persistLibraryCache();
        SF_LOG_I("RomM", "Virtual library: added %zu missing row(s), removed %zu duplicate row(s)",
                 insertedRows, deduped);
    }

    SF_LOG_I("RomM", "Virtual library: %zu entries (%zu downloaded)", total, downloaded);
}

void RomMService::refreshLibraryAsync(std::function<void(bool, const std::string&)> done) {
    refreshConfig();
    if (!enabled_ || !client_.isReady()) {
        if (done) done(false, "RomM not configured or authenticated");
        return;
    }

    AppState::instance().pool().enqueue([this, done = std::move(done)]() mutable {
        std::vector<LibraryEntry> out;
        bool ok = library_->fetchLibrary(out);
        std::string message = ok ? "Library refreshed" : client_.lastError();

        if (done) {
            brls::sync([done = std::move(done), ok, message]() mutable {
                done(ok, message);
            });
        }
    });
}

void RomMService::downloadRomAsync(const LibraryEntry& entry,
                                   std::function<void(const std::string&, int)> progress,
                                   std::atomic<bool>* abort,
                                   std::function<void(bool, const std::string&)> done,
                                   const std::vector<long long>& fileIds) {
    // Do not bail out on an empty file list here: the worker resolves the file list from
    // /roms/{id}/simple when the paginated response omitted it.
    AppState::instance().pool().enqueue([this, entry, fileIds, progress, abort, done = std::move(done)]() mutable {
        const std::string destDir = library_->downloadDirFor(entry.matchedSystemId);

        auto wrappedProgress = [progress](const DownloadProgress& p) -> bool {
            if (!progress) return false;

            const int filePercent = p.totalBytes > 0
                                        ? static_cast<int>(p.downloadedBytes * 100 / p.totalBytes)
                                        : 0;
            const int percent = p.totalFiles > 0
                                    ? ((p.fileIndex - 1) * 100 + filePercent) / p.totalFiles
                                    : 0;

            std::string message;
            const std::string verb = p.resumed ? "Resuming " : "Downloading ";
            if (p.totalBytes > 0) {
                message = verb + p.fileName + " (" + std::to_string(filePercent) + "%, " +
                          formatSize(p.downloadedBytes) + " / " + formatSize(p.totalBytes) + ")";
            } else if (p.downloadedBytes > 0) {
                // Chunked response with no Content-Length: report bytes so the UI keeps moving.
                message = verb + p.fileName + " (" + formatSize(p.downloadedBytes) + ")";
            } else {
                message = "File " + std::to_string(p.fileIndex) + "/" + std::to_string(p.totalFiles) +
                          ": " + p.fileName;
            }

            brls::sync([progress, message, percent]() { progress(message, percent); });
            return false;
        };

        bool ok = library_->downloadRomFiles(entry, fileIds, destDir, wrappedProgress, abort);
        std::string message;
        if (ok) {
            // Register the mapping so save/state sync works for this ROM going forward.
            const SystemConfig* sys = sf::Config::instance().findSystem(entry.matchedSystemId);
            const LibraryEntry* saved = library_->entryByRomId(entry.rom.id);
            const std::string localPath = saved && !saved->localPath.empty()
                ? saved->localPath
                : FileSystem::join(destDir, entry.rom.files.front().fileName);
            mapper_->addOrUpdateMapping(entry.rom.id, entry.matchedSystemId, localPath,
                                        entry.rom.name, sys ? sys->core : "");
            message = "Downloaded to " + localPath;
        } else {
            message = abort && abort->load() ? "Download cancelled" : client_.lastError();
            if (message.empty())
                message = "Download failed — could not resolve the ROM's files";
        }

        if (done) {
            brls::sync([done = std::move(done), ok, message]() mutable {
                done(ok, message);
            });
        }
    });
}

void RomMService::syncAll(std::function<void(bool, const std::string&)> done,
                          std::function<void(const std::string&, int)> progress) {
    refreshConfig();
    if (!enabled_) {
        if (done) done(false, "RomM sync not enabled");
        return;
    }

    if (syncInProgress_.exchange(true)) {
        if (done) done(false, "Sync already in progress");
        return;
    }

    std::string error;
    if (!ensureAuthenticated(error)) {
        syncInProgress_.store(false);
        if (done) done(false, "Auth error: " + error);
        return;
    }

    AppState::instance().pool().enqueue([this, progress, done = std::move(done)]() mutable {
        auto detailed = [progress](const SyncDetailedProgress& p) {
            if (!progress) return;

            std::string message = p.message;
            if (p.totalFiles > 1)
                message += " — ROM " + std::to_string(p.currentFile + 1) + "/" + std::to_string(p.totalFiles);

            brls::sync([progress, message, percent = p.percent]() { progress(message, percent); });
        };

        std::atomic<bool> abort{false};

        bool result = syncEngine_->syncAll(detailed, &abort);
        std::string message = syncEngine_->lastResult();
        lastSyncResult_ = message;

        syncInProgress_.store(false);

        if (done) {
            brls::sync([done = std::move(done), result, message]() mutable {
                done(result, message);
            });
        }
    });
}

bool RomMService::pairDevice(const std::string& code, std::string& errorOut) {
    // Refresh config to get latest server URL from settings
    refreshConfig();
    
    if (config_.serverUrl.empty()) {
        errorOut = "Server URL not configured";
        return false;
    }

    // Configure client with just server URL for pairing
    client_.configure(config_.serverUrl);

    PairingResponse pairingResp;
    if (!client_.exchangePairingCode(code, pairingResp)) {
        errorOut = client_.lastError();
        return false;
    }

    // Now register device using the access token from pairing
    client_.configure(config_.serverUrl, pairingResp.accessToken);

    Device device;
    if (!client_.registerDevice(config_.deviceName.empty() ? "NXStation" : config_.deviceName, device)) {
        errorOut = client_.lastError();
        return false;
    }

    // Save tokens and device ID
    config_.deviceId = device.id;
    config_.accessToken = pairingResp.accessToken;
    config_.clientToken = pairingResp.clientToken;
    config_.tokenExpiresAt = pairingResp.expiresAt;

    store_->setDeviceId(device.id);
    store_->setClientToken(pairingResp.clientToken);
    store_->setAccessToken(pairingResp.accessToken);
    store_->setTokenExpiresAt(pairingResp.expiresAt);
    store_->save();

    // Reconfigure client with full auth
    client_.configure(config_.serverUrl, pairingResp.accessToken, pairingResp.clientToken);

    enabled_ = true;
    SF_LOG_I("RomM", "Device paired successfully: %s", device.id.c_str());
    return true;
}

bool RomMService::disconnect(std::string& errorOut) {
    refreshConfig();
    // Clear local state
    config_.deviceId.clear();
    config_.accessToken.clear();
    config_.clientToken.clear();
    config_.tokenExpiresAt = 0;

    store_->setDeviceId("");
    store_->setClientToken("");
    store_->setAccessToken("");
    store_->setTokenExpiresAt(0);
    store_->save();

    client_.configure(config_.serverUrl);
    enabled_ = false;

    SF_LOG_I("RomM", "Device disconnected");
    return true;
}

bool RomMService::isPaired() {
    refreshConfig();
    return !config_.deviceId.empty() && !config_.accessToken.empty();
}

std::string RomMService::deviceId() {
    refreshConfig();
    return config_.deviceId;
}

} // namespace sf::romm
