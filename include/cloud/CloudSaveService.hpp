#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace sf::cloud {

struct CloudSyncResult {
    bool ok = false;
    std::string message;
    size_t filesZipped = 0;
};

struct CloudBackupInfo {
    std::string id;
    std::string name;
    int64_t size = 0;
    std::string displayLabel;
};

struct CloudRestoreProgress {
    enum class Kind {
        Info,
        PreBackup,
        Download,
        Extract,
        Summary,
    };

    Kind kind = Kind::Info;
    std::string message;
    std::string detail;
    int percent = 0;
    bool fileOk = true;
    bool skipped = false;
};

struct CloudRestoreResult {
    bool ok = false;
    bool aborted = false;
    std::string message;
    size_t filesRestored = 0;
    size_t filesSkipped = 0;
    size_t filesFailed = 0;
    std::string preRestoreZipPath;
};

using CloudProgressCallback = std::function<void(const std::string& stage, int percent)>;
using CloudRestoreProgressCallback = std::function<void(const CloudRestoreProgress& progress)>;

/** RetroArch save/state backup to Google Drive (JKSV-style OAuth). */
class CloudSaveService {
public:
    static CloudSaveService& instance();

    bool oauthConfigPresent() const;
    bool isLinked() const;
    bool isEnabled() const;
    bool isSyncInProgress() const;

    /** Run full sync on worker thread; callback on main thread via brls::sync. */
    void syncNow(CloudProgressCallback progress, std::function<void(CloudSyncResult)> done);

    /** List Drive backups on worker thread. */
    void listBackups(std::function<void(std::vector<CloudBackupInfo>, std::string error)> done);

    /** Restore a Drive backup on worker thread. */
    void restoreNow(const std::string& backupId, const std::string& backupName,
                    CloudRestoreProgressCallback progress, std::atomic<bool>* abort,
                    std::function<void(CloudRestoreResult)> done);

    /** If auto-save enabled and account linked, queue sync (non-blocking). */
    void maybeAutoSyncAfterReturn();

private:
    CloudSaveService() = default;

    static std::atomic<bool> syncInProgress_;

    CloudSyncResult runSync(CloudProgressCallback progress);
    CloudRestoreResult runRestore(const std::string& backupId, const std::string& backupName,
                                  CloudRestoreProgressCallback progress, std::atomic<bool>* abort);
    std::string makeBackupFileName() const;
    std::string makePreRestoreZipPath() const;
};

} // namespace sf::cloud
