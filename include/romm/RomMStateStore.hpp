#pragma once

#include "romm/RomMConfig.hpp"
#include "romm/RomMClient.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace sf::romm {

/** Fingerprint of a local save file for change detection. */
struct FileFingerprint {
    std::string hash;
    int64_t size = 0;
    time_t modifiedTime = 0;
    std::string rommSaveId;  // RomM save ID if synced
};

/** Pending sync operation for crash recovery. */
struct PendingOperation {
    enum class Type { Upload, Download, Delete, ConfirmDownload };
    Type type;
    std::string rommSaveId;
    std::string localPath;
    std::string sessionId;
    std::string hash;  // For confirm download
};

/** RomM state store - persists sync metadata atomically. */
class RomMStateStore {
public:
    RomMStateStore(const RomMConfig& config);

    /** Load state from disk. */
    bool load();

    /** Save state to disk atomically. */
    bool save();

    // Device registration
    std::string deviceId() const { return deviceId_; }
    void setDeviceId(const std::string& id) { deviceId_ = id; }

    std::string clientToken() const { return clientToken_; }
    void setClientToken(const std::string& token) { clientToken_ = token; }

    std::string accessToken() const { return accessToken_; }
    void setAccessToken(const std::string& token) { accessToken_ = token; }

    time_t tokenExpiresAt() const { return tokenExpiresAt_; }
    void setTokenExpiresAt(time_t t) { tokenExpiresAt_ = t; }

    // Sync session tracking
    std::string lastSessionId() const { return lastSessionId_; }
    void setLastSessionId(const std::string& id) { lastSessionId_ = id; }

    time_t lastSuccessfulSync() const { return lastSuccessfulSync_; }
    void setLastSuccessfulSync(time_t t) { lastSuccessfulSync_ = t; }

    std::string lastSyncResult() const { return lastSyncResult_; }
    void setLastSyncResult(const std::string& msg) { lastSyncResult_ = msg; }

    // File fingerprints
    std::optional<FileFingerprint> getFingerprint(const std::string& localPath) const;
    void setFingerprint(const std::string& localPath, const FileFingerprint& fp);
    void removeFingerprint(const std::string& localPath);
    const std::unordered_map<std::string, FileFingerprint>& allFingerprints() const { return fingerprints_; }

    // Pending operations (for crash recovery)
    void addPendingOperation(const PendingOperation& op);
    void removePendingOperation(const std::string& rommSaveId, PendingOperation::Type type);
    std::vector<PendingOperation> getPendingOperations() const { return pendingOps_; }
    void clearPendingOperations() { pendingOps_.clear(); }

    // Conflict backups
    void recordConflictBackup(const std::string& localPath, const std::string& backupPath);
    std::optional<std::string> getConflictBackup(const std::string& localPath) const;

private:
    const RomMConfig& config_;
    std::string deviceId_;
    std::string clientToken_;
    std::string accessToken_;
    time_t tokenExpiresAt_ = 0;
    std::string lastSessionId_;
    time_t lastSuccessfulSync_ = 0;
    std::string lastSyncResult_;

    std::unordered_map<std::string, FileFingerprint> fingerprints_;
    std::vector<PendingOperation> pendingOps_;
    std::unordered_map<std::string, std::string> conflictBackups_;

    std::string stateFilePath() const;
    std::string tempStateFilePath() const;
    bool writeAtomic(const std::string& content);
};

} // namespace sf::romm
