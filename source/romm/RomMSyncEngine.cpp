#include "romm/RomMSyncEngine.hpp"
#include "romm/RomMClient.hpp"
#include "romm/RomMSaveMapper.hpp"
#include "romm/RomMStateStore.hpp"
#include "romm/RomMConfig.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Hash.hpp"

#include <algorithm>
#include <borealis.hpp>

namespace sf::romm {

RomMSyncEngine::RomMSyncEngine(RomMClient& client, RomMSaveMapper& mapper, RomMStateStore& store,
                               const RomMConfig& config)
    : client_(client), mapper_(mapper), store_(store), config_(config) {}

bool RomMSyncEngine::syncForRom(const std::string& romPath, const std::string& systemId,
                                const std::string& coreId, SyncDetailedCallback progress,
                                std::atomic<bool>* abort) {
    if (syncInProgress_.exchange(true)) {
        lastResult_ = "Sync already in progress";
        return false;
    }

    SyncContext ctx;
    ctx.romPath = romPath;
    ctx.systemId = systemId;
    ctx.coreId = coreId;
    ctx.progress = std::move(progress);
    ctx.abort = abort;

    bool result = runSyncForRom(ctx);
    syncInProgress_.store(false);
    return result;
}

bool RomMSyncEngine::syncAll(SyncDetailedCallback progress, std::atomic<bool>* abort) {
    if (syncInProgress_.exchange(true)) {
        lastResult_ = "Sync already in progress";
        return false;
    }

    // Sync every mapped ROM
    const auto mappings = mapper_.allMappings();
    if (mappings.empty()) {
        syncInProgress_.store(false);
        lastResult_ = "Nothing to sync yet — download a ROM from the RomM library first";
        return true;
    }

    size_t synced = 0;
    size_t failed = 0;
    const size_t total = mappings.size();
    size_t completed = 0;

    for (const auto& mapping : mappings) {
        if (abort && abort->load()) break;

        SyncContext ctx;
        ctx.romPath = mapping.romPath;
        ctx.systemId = mapping.systemId;
        ctx.coreId = mapping.coreId;
        ctx.progress = [&, completed](const SyncDetailedProgress& p) {
            if (progress) {
                SyncDetailedProgress adapted = p;
                adapted.currentFile = completed;
                adapted.totalFiles = total;
                progress(adapted);
            }
        };
        ctx.abort = abort;

        if (runSyncForRom(ctx))
            ++synced;
        else
            ++failed;
        ++completed;
    }

    syncInProgress_.store(false);

    if (abort && abort->load())
        lastResult_ = "Sync cancelled (" + std::to_string(synced) + " of " + std::to_string(total) + ")";
    else {
        lastResult_ = "Synced " + std::to_string(synced) + " of " + std::to_string(total) + " ROMs";
        if (failed > 0)
            lastResult_ += " (" + std::to_string(failed) + " failed)";
    }

    store_.setLastSyncResult(lastResult_);
    store_.save();

    return failed == 0;
}

bool RomMSyncEngine::runSyncForRom(SyncContext& ctx) {
    lastResult_.clear();

    emitProgress(ctx, SyncDetailedProgress::Stage::Connecting, "Connecting to RomM…");

    // Find mapping
    auto mappingOpt = mapper_.findMapping(ctx.romPath, ctx.systemId);
    if (!mappingOpt) {
        lastResult_ = "No RomM mapping for this ROM";
        emitProgress(ctx, SyncDetailedProgress::Stage::Error, lastResult_);
        return false;
    }
    ctx.rommRomId = mappingOpt->rommRomId;

    // Collect local files
    emitProgress(ctx, SyncDetailedProgress::Stage::Negotiating, "Scanning local saves…");
    if (!collectLocalFiles(ctx)) return false;

    // Negotiate with server
    emitProgress(ctx, SyncDetailedProgress::Stage::Negotiating, "Negotiating with server…");
    if (!negotiate(ctx)) return false;

    // Handle downloads
    if (!ctx.negotiateResp.downloads.empty()) {
        emitProgress(ctx, SyncDetailedProgress::Stage::Downloading, "Downloading saves…");
        if (!handleDownloads(ctx)) return false;
    }

    // Handle uploads
    if (!ctx.negotiateResp.uploads.empty()) {
        emitProgress(ctx, SyncDetailedProgress::Stage::Uploading, "Uploading saves…");
        if (!handleUploads(ctx)) return false;
    }

    // Handle conflicts
    if (!ctx.negotiateResp.conflicts.empty()) {
        emitProgress(ctx, SyncDetailedProgress::Stage::ResolvingConflicts, "Resolving conflicts…");
        if (!handleConflicts(ctx)) return false;
    }

    // Handle deletes
    if (!ctx.negotiateResp.deletes.empty()) {
        emitProgress(ctx, SyncDetailedProgress::Stage::ResolvingConflicts, "Processing deletions…");
        if (!handleDeletes(ctx)) return false;
    }

    // Complete session
    emitProgress(ctx, SyncDetailedProgress::Stage::Completing, "Finalizing sync…");
    if (!completeSession(ctx)) return false;

    // Update state store
    store_.setLastSuccessfulSync(std::time(nullptr));
    store_.setLastSyncResult(lastResult_);
    store_.save();

    emitProgress(ctx, SyncDetailedProgress::Stage::Done, "Sync complete", lastResult_, 100);
    return true;
}

bool RomMSyncEngine::collectLocalFiles(SyncContext& ctx) {
    if (ctx.abort && ctx.abort->load()) return false;

    auto localFiles = mapper_.discoverSaves(ctx.romPath, ctx.systemId, ctx.coreId);

    // Convert LocalSaveFile to RomMSave for negotiation
    ctx.localSaves.clear();
    ctx.localSaves.reserve(localFiles.size());
    for (const auto& local : localFiles) {
        RomMSave save;
        save.romId = local.rommRomId;
        save.slot = local.slot;
        save.deviceId = config_.deviceId;
        save.type = local.type;
        save.hash = local.hash;
        save.size = local.size;
        save.coreId = local.coreId;
        ctx.localSaves.push_back(std::move(save));
    }

    // Filter by config
    if (!config_.syncSaves) {
        ctx.localSaves.erase(
            std::remove_if(ctx.localSaves.begin(), ctx.localSaves.end(),
                           [](const RomMSave& s) { return s.type == "save"; }),
            ctx.localSaves.end());
    }
    if (!config_.syncStates) {
        ctx.localSaves.erase(
            std::remove_if(ctx.localSaves.begin(), ctx.localSaves.end(),
                           [](const RomMSave& s) { return s.type == "state"; }),
            ctx.localSaves.end());
    }

    SF_LOG_I("RomM", "Found %zu local save files for %s", ctx.localSaves.size(), ctx.romPath.c_str());
    return true;
}

bool RomMSyncEngine::negotiate(SyncContext& ctx) {
    if (ctx.abort && ctx.abort->load()) return false;

    if (!client_.negotiate(ctx.localSaves, ctx.negotiateResp)) {
        lastResult_ = "Negotiation failed: " + client_.lastError();
        emitProgress(ctx, SyncDetailedProgress::Stage::Error, lastResult_);
        return false;
    }

    SF_LOG_I("RomM", "Negotiation: %zu uploads, %zu downloads, %zu conflicts, %zu deletes",
             ctx.negotiateResp.uploads.size(), ctx.negotiateResp.downloads.size(),
             ctx.negotiateResp.conflicts.size(), ctx.negotiateResp.deletes.size());
    return true;
}

bool RomMSyncEngine::handleDownloads(SyncContext& ctx) {
    if (ctx.abort && ctx.abort->load()) return false;

    size_t total = ctx.negotiateResp.downloads.size();
    size_t completed = 0;
    std::vector<std::string> completedIds;
    std::vector<std::string> failedIds;

    for (const auto& save : ctx.negotiateResp.downloads) {
        if (ctx.abort && ctx.abort->load()) break;

        emitProgress(ctx, SyncDetailedProgress::Stage::Downloading,
                     "Downloading " + save.type + " (" + save.slot + ")",
                     save.id, -1, completed, total);

        auto localPathOpt = mapper_.resolveLocalPath(save);
        if (!localPathOpt) {
            SF_LOG_W("RomM", "Cannot resolve local path for save %s", save.id.c_str());
            failedIds.push_back(save.id);
            continue;
        }

        const std::string localPath = *localPathOpt;
        const std::string tempPath = localPath + ".romm_download";

        // Download to temp file
        if (!client_.downloadSave(save.id, tempPath)) {
            SF_LOG_E("RomM", "Failed to download save %s: %s", save.id.c_str(), client_.lastError().c_str());
            failedIds.push_back(save.id);
            continue;
        }

        // Verify hash
        const std::string downloadedHash = computeFileHash(tempPath);
        if (downloadedHash != save.hash) {
            SF_LOG_E("RomM", "Hash mismatch for save %s: expected %s, got %s",
                     save.id.c_str(), save.hash.c_str(), downloadedHash.c_str());
            FileSystem::removeFile(tempPath);
            failedIds.push_back(save.id);
            continue;
        }

        // Handle conflict policy
        if (FileSystem::exists(localPath)) {
            const std::string localHash = computeFileHash(localPath);
            if (localHash != save.hash) {
                // Conflict!
                if (config_.conflictPolicy == ConflictPolicy::KeepBoth) {
                    // Archive local copy
                    std::string backupPath = localPath + ".conflict_" + std::to_string(std::time(nullptr));
                    if (FileSystem::renameFile(localPath, backupPath)) {
                        store_.recordConflictBackup(localPath, backupPath);
                        SF_LOG_I("RomM", "Archived conflicting local save to %s", backupPath.c_str());
                    }
                } else if (config_.conflictPolicy == ConflictPolicy::KeepLocal) {
                    SF_LOG_I("RomM", "Keeping local save (conflict policy: keep_local)");
                    FileSystem::removeFile(tempPath);
                    failedIds.push_back(save.id);
                    continue;
                }
                // KeepRemote: overwrite (default)
            }
        }

        // Atomic replace
        if (!writeFileAtomic(localPath, tempPath)) {
            SF_LOG_E("RomM", "Failed to write save file %s", localPath.c_str());
            failedIds.push_back(save.id);
            continue;
        }

        // Update fingerprint
        FileFingerprint fp;
        fp.hash = downloadedHash;
        fp.size = save.size;
        fp.modifiedTime = std::time(nullptr);
        fp.rommSaveId = save.id;
        store_.setFingerprint(localPath, fp);

        // Confirm download
        if (!client_.confirmDownload(save.id, downloadedHash)) {
            SF_LOG_W("RomM", "Failed to confirm download for %s", save.id.c_str());
        }

        completedIds.push_back(save.id);
        ++completed;
    }

    lastResult_ = "Downloaded " + std::to_string(completed) + " save(s)";
    if (!failedIds.empty()) {
        lastResult_ += ", " + std::to_string(failedIds.size()) + " failed";
    }
    return failedIds.empty();
}

bool RomMSyncEngine::handleUploads(SyncContext& ctx) {
    if (ctx.abort && ctx.abort->load()) return false;

    size_t total = ctx.negotiateResp.uploads.size();
    size_t completed = 0;
    std::vector<std::string> completedIds;
    std::vector<std::string> failedIds;

    for (const auto& save : ctx.negotiateResp.uploads) {
        if (ctx.abort && ctx.abort->load()) break;

        emitProgress(ctx, SyncDetailedProgress::Stage::Uploading,
                     "Uploading " + save.type + " (" + save.slot + ")",
                     save.id, -1, completed, total);

        // Find local file
        auto localPathOpt = mapper_.resolveLocalPath(save);
        if (!localPathOpt) {
            SF_LOG_W("RomM", "Local file not found for upload %s", save.id.c_str());
            failedIds.push_back(save.id);
            continue;
        }

        const std::string localPath = *localPathOpt;

        // Verify local file still matches
        const std::string localHash = computeFileHash(localPath);
        if (localHash != save.hash) {
            SF_LOG_W("RomM", "Local file changed since negotiation for %s", save.id.c_str());
            failedIds.push_back(save.id);
            continue;
        }

        UploadResponse uploadResp;
        if (!client_.uploadSave(localPath, save, uploadResp)) {
            SF_LOG_E("RomM", "Failed to upload save %s: %s", save.id.c_str(), client_.lastError().c_str());
            failedIds.push_back(save.id);
            continue;
        }

        // Update fingerprint with new RomM save ID
        FileFingerprint fp;
        fp.hash = localHash;
        fp.size = save.size;
        fp.modifiedTime = std::time(nullptr);
        fp.rommSaveId = uploadResp.saveId;
        store_.setFingerprint(localPath, fp);

        completedIds.push_back(uploadResp.saveId);
        ++completed;
    }

    if (!failedIds.empty()) {
        lastResult_ = "Uploaded " + std::to_string(completed) + " save(s), " +
                      std::to_string(failedIds.size()) + " failed";
    } else {
        lastResult_ = "Uploaded " + std::to_string(completed) + " save(s)";
    }
    return failedIds.empty();
}

bool RomMSyncEngine::handleConflicts(SyncContext& ctx) {
    // Conflicts are reported by server when local and remote both changed
    // With keep_both policy, we already archived local during download
    // Here we just acknowledge them
    for (const auto& save : ctx.negotiateResp.conflicts) {
        SF_LOG_I("RomM", "Conflict for save %s (slot %s) - local archived", save.id.c_str(), save.slot.c_str());
    }
    return true;
}

bool RomMSyncEngine::handleDeletes(SyncContext& ctx) {
    // Server says these saves should be deleted locally
    for (const auto& save : ctx.negotiateResp.deletes) {
        auto localPathOpt = mapper_.resolveLocalPath(save);
        if (localPathOpt && FileSystem::exists(*localPathOpt)) {
            if (config_.conflictPolicy == ConflictPolicy::KeepBoth) {
                std::string backupPath = *localPathOpt + ".deleted_" + std::to_string(std::time(nullptr));
                FileSystem::renameFile(*localPathOpt, backupPath);
                store_.recordConflictBackup(*localPathOpt, backupPath);
            } else {
                FileSystem::removeFile(*localPathOpt);
            }
            store_.removeFingerprint(*localPathOpt);
        }
    }
    return true;
}

bool RomMSyncEngine::completeSession(SyncContext& ctx) {
    if (ctx.abort && ctx.abort->load()) return false;

    std::vector<std::string> completed;
    std::vector<std::string> failed;

    // Collect completed operation IDs
    for (const auto& save : ctx.negotiateResp.downloads) completed.push_back(save.id);
    for (const auto& save : ctx.negotiateResp.uploads) completed.push_back(save.id);
    for (const auto& save : ctx.negotiateResp.conflicts) completed.push_back(save.id);
    for (const auto& save : ctx.negotiateResp.deletes) completed.push_back(save.id);

    if (!client_.completeSyncSession(ctx.negotiateResp.sessionId, completed, failed)) {
        SF_LOG_E("RomM", "Failed to complete sync session: %s", client_.lastError().c_str());
        // Don't fail the whole sync for this
    }

    store_.setLastSessionId(ctx.negotiateResp.sessionId);
    store_.clearPendingOperations();
    return true;
}

void RomMSyncEngine::emitProgress(SyncContext& ctx, SyncDetailedProgress::Stage stage,
                                  const std::string& message, const std::string& detail,
                                  int percent, size_t current, size_t total) {
    if (!ctx.progress) return;

    SyncDetailedProgress p;
    p.stage = stage;
    p.message = message;
    p.detail = detail;
    p.percent = percent >= 0 ? percent : (total > 0 ? static_cast<int>((current * 100) / total) : 0);
    p.currentFile = current;
    p.totalFiles = total;

    // Ensure callback runs on main thread via brls::sync
    brls::sync([cb = ctx.progress, p]() { cb(p); });
}

std::string RomMSyncEngine::computeFileHash(const std::string& path) {
    return sf::Hash::md5File(path);
}

bool RomMSyncEngine::writeFileAtomic(const std::string& path, const std::string& tempPath) {
    FileSystem::createDirectories(FileSystem::parentPath(path));
    // Overwrite: a save/state file may already exist from a previous sync.
    return FileSystem::renameFileOverwrite(tempPath, path);
}

} // namespace sf::romm
