#include "romm/RomMStateStore.hpp"
#include "romm/RomMConfig.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <ctime>
#include <fstream>

namespace sf::romm {

RomMStateStore::RomMStateStore(const RomMConfig& config) : config_(config) {
    load();
}

bool RomMStateStore::load() {
    const std::string path = stateFilePath();
    if (!FileSystem::exists(path)) {
        SF_LOG_I("RomM", "No sync state file at %s", path.c_str());
        return true;
    }

    try {
        const auto j = Json::parse(FileSystem::readFile(path));

        deviceId_ = j.value("device_id", std::string());
        clientToken_ = j.value("client_token", std::string());
        accessToken_ = j.value("access_token", std::string());
        tokenExpiresAt_ = static_cast<time_t>(j.value("token_expires_at", 0.0));
        lastSessionId_ = j.value("last_session_id", std::string());
        lastSuccessfulSync_ = static_cast<time_t>(j.value("last_successful_sync", 0.0));
        lastSyncResult_ = j.value("last_sync_result", std::string());

        if (j.contains("fingerprints") && j["fingerprints"].is_object()) {
            for (const auto& [path, fp] : j["fingerprints"].items()) {
                FileFingerprint f;
                f.hash = fp.value("hash", std::string());
                f.size = static_cast<int64_t>(fp.value("size", 0.0));
                f.modifiedTime = static_cast<time_t>(fp.value("modified_time", 0.0));
                f.rommSaveId = fp.value("romm_save_id", std::string());
                fingerprints_[path] = std::move(f);
            }
        }

        if (j.contains("pending_operations") && j["pending_operations"].is_array()) {
            for (const auto& op : j["pending_operations"]) {
                PendingOperation p;
                p.type = static_cast<PendingOperation::Type>(op.value("type", 0));
                p.rommSaveId = op.value("romm_save_id", std::string());
                p.localPath = op.value("local_path", std::string());
                p.sessionId = op.value("session_id", std::string());
                p.hash = op.value("hash", std::string());
                pendingOps_.push_back(std::move(p));
            }
        }

        if (j.contains("conflict_backups") && j["conflict_backups"].is_object()) {
            for (const auto& [local, backup] : j["conflict_backups"].items()) {
                if (backup.is_string()) {
                    conflictBackups_[local] = backup.get<std::string>();
                }
            }
        }

        SF_LOG_I("RomM", "Loaded sync state: device=%s, fingerprints=%zu, pending=%zu",
                 deviceId_.c_str(), fingerprints_.size(), pendingOps_.size());
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_E("RomM", "Failed to load sync state: %s", ex.what());
        return false;
    }
}

bool RomMStateStore::save() {
    Json root = Json::object();
    root["device_id"] = deviceId_;
    root["client_token"] = clientToken_;
    root["access_token"] = accessToken_;
    root["token_expires_at"] = static_cast<double>(tokenExpiresAt_);
    root["last_session_id"] = lastSessionId_;
    root["last_successful_sync"] = static_cast<double>(lastSuccessfulSync_);
    root["last_sync_result"] = lastSyncResult_;

    Json fps = Json::object();
    for (const auto& [path, fp] : fingerprints_) {
        Json f = Json::object();
        f["hash"] = fp.hash;
        f["size"] = static_cast<double>(fp.size);
        f["modified_time"] = static_cast<double>(fp.modifiedTime);
        f["romm_save_id"] = fp.rommSaveId;
        fps[path] = std::move(f);
    }
    root["fingerprints"] = std::move(fps);

    Json pending = Json::array();
    for (const auto& op : pendingOps_) {
        Json p = Json::object();
        p["type"] = static_cast<int>(op.type);
        p["romm_save_id"] = op.rommSaveId;
        p["local_path"] = op.localPath;
        p["session_id"] = op.sessionId;
        p["hash"] = op.hash;
        pending.push_back(std::move(p));
    }
    root["pending_operations"] = std::move(pending);

    Json conflicts = Json::object();
    for (const auto& [local, backup] : conflictBackups_) {
        conflicts[local] = backup;
    }
    root["conflict_backups"] = std::move(conflicts);

    return writeAtomic(root.dump(2));
}

std::string RomMStateStore::stateFilePath() const {
    return std::string(paths::DATA_DIR) + "/romm_sync_state.json";
}

std::string RomMStateStore::tempStateFilePath() const {
    return stateFilePath() + ".tmp";
}

bool RomMStateStore::writeAtomic(const std::string& content) {
    const std::string tempPath = tempStateFilePath();
    const std::string finalPath = stateFilePath();

    FileSystem::createDirectories(FileSystem::parentPath(finalPath));

    if (!FileSystem::writeFile(tempPath, content)) {
        SF_LOG_E("RomM", "Failed to write temp state file");
        return false;
    }

    // Atomic rename (overwrite: the state file already exists on subsequent runs)
    if (!FileSystem::renameFileOverwrite(tempPath, finalPath)) {
        SF_LOG_E("RomM", "Failed to rename state file");
        FileSystem::removeFile(tempPath);
        return false;
    }
    return true;
}

std::optional<FileFingerprint> RomMStateStore::getFingerprint(const std::string& localPath) const {
    auto it = fingerprints_.find(localPath);
    if (it != fingerprints_.end()) return it->second;
    return std::nullopt;
}

void RomMStateStore::setFingerprint(const std::string& localPath, const FileFingerprint& fp) {
    fingerprints_[localPath] = fp;
}

void RomMStateStore::removeFingerprint(const std::string& localPath) {
    fingerprints_.erase(localPath);
}

void RomMStateStore::addPendingOperation(const PendingOperation& op) {
    pendingOps_.push_back(op);
}

void RomMStateStore::removePendingOperation(const std::string& rommSaveId, PendingOperation::Type type) {
    pendingOps_.erase(
        std::remove_if(pendingOps_.begin(), pendingOps_.end(),
                       [&](const PendingOperation& op) {
                           return op.rommSaveId == rommSaveId && op.type == type;
                       }),
        pendingOps_.end());
}

void RomMStateStore::recordConflictBackup(const std::string& localPath, const std::string& backupPath) {
    conflictBackups_[localPath] = backupPath;
}

std::optional<std::string> RomMStateStore::getConflictBackup(const std::string& localPath) const {
    auto it = conflictBackups_.find(localPath);
    if (it != conflictBackups_.end()) return it->second;
    return std::nullopt;
}

} // namespace sf::romm
