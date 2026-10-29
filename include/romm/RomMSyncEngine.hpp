#pragma once

#include "romm/RomMClient.hpp"
#include "romm/RomMConfig.hpp"
#include "romm/RomMSaveMapper.hpp"
#include "romm/RomMStateStore.hpp"

#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace sf::romm {

/** Progress callback for sync operations. */
using SyncProgressCallback = std::function<void(const std::string& stage, int percent)>;

/** Detailed progress for UI. */
struct SyncDetailedProgress {
    enum class Stage {
        Idle,
        Connecting,
        Negotiating,
        Downloading,
        Uploading,
        ResolvingConflicts,
        Completing,
        Done,
        Error
    };
    Stage stage = Stage::Idle;
    std::string message;
    std::string detail;
    int percent = 0;
    size_t currentFile = 0;
    size_t totalFiles = 0;
};

using SyncDetailedCallback = std::function<void(const SyncDetailedProgress&)>;

/** RomM sync engine - orchestrates the sync protocol. */
class RomMSyncEngine {
public:
    RomMSyncEngine(RomMClient& client, RomMSaveMapper& mapper, RomMStateStore& store,
                   const RomMConfig& config);

    /** Run a full sync for a specific ROM (by NXStation game path). */
    bool syncForRom(const std::string& romPath, const std::string& systemId,
                    const std::string& coreId, SyncDetailedCallback progress = {},
                    std::atomic<bool>* abort = nullptr);

    /** Run a full sync for all configured ROMs. */
    bool syncAll(SyncDetailedCallback progress = {}, std::atomic<bool>* abort = nullptr);

    /** Get last sync result summary. */
    std::string lastResult() const { return lastResult_; }

    /** Check if sync is in progress. */
    bool isSyncInProgress() const { return syncInProgress_.load(); }

private:
    RomMClient& client_;
    RomMSaveMapper& mapper_;
    RomMStateStore& store_;
    const RomMConfig& config_;

    std::atomic<bool> syncInProgress_{false};
    std::string lastResult_;

    struct SyncContext {
        std::string romPath;
        std::string systemId;
        std::string coreId;
        std::string rommRomId;
        std::string sessionId;
        std::vector<RomMSave> localSaves;
        std::vector<RomMSave> localStates;
        NegotiateResponse negotiateResp;
        SyncDetailedCallback progress;
        std::atomic<bool>* abort = nullptr;
    };

    bool runSyncForRom(SyncContext& ctx);
    bool collectLocalFiles(SyncContext& ctx);
    bool negotiate(SyncContext& ctx);
    bool handleDownloads(SyncContext& ctx);
    bool handleUploads(SyncContext& ctx);
    bool handleConflicts(SyncContext& ctx);
    bool handleDeletes(SyncContext& ctx);
    bool completeSession(SyncContext& ctx);

    void emitProgress(SyncContext& ctx, SyncDetailedProgress::Stage stage,
                      const std::string& message, const std::string& detail = "",
                      int percent = -1, size_t current = 0, size_t total = 0);

    std::string computeFileHash(const std::string& path);
    bool writeFileAtomic(const std::string& path, const std::string& tempPath);
};

} // namespace sf::romm
