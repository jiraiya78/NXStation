#include "cloud/CloudSaveService.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "cloud/GoogleDriveClient.hpp"
#include "cloud/RetroArchPaths.hpp"
#include "cloud/ZipArchive.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Network.hpp"
#include "util/Paths.hpp"
#include "util/CloudLog.hpp"

#include <borealis.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <atomic>

namespace sf::cloud {

std::atomic<bool> CloudSaveService::syncInProgress_{false};

namespace {

struct CloudLogSession {
    CloudLogSession(const char* title)
    {
        sf::CloudLog::open();
        sf::CloudLog::writef("=== %s ===", title);
    }
    ~CloudLogSession() { sf::CloudLog::close(); }
};

std::string formatBackupLabel(const std::string& fileName)
{
    const std::string prefix = "retroarch-saves-";
    const std::string suffix = ".zip";
    if (fileName.rfind(prefix, 0) != 0 || fileName.size() <= prefix.size() + suffix.size())
        return fileName;

    const std::string stamp = fileName.substr(prefix.size(), fileName.size() - prefix.size() - suffix.size());
    std::tm tm {};
    std::istringstream in(stamp);
    in >> std::get_time(&tm, "%Y-%m-%d-%H%M%S");
    if (in.fail())
        return fileName;

    std::ostringstream out;
    out << std::put_time(&tm, "%b %d, %Y  %H:%M:%S");
    return out.str();
}

} // namespace

CloudSaveService& CloudSaveService::instance()
{
    static CloudSaveService service;
    return service;
}

bool CloudSaveService::oauthConfigPresent() const
{
    GoogleDriveClient client;
    return client.oauthConfigPresent();
}

bool CloudSaveService::isLinked() const
{
    GoogleDriveClient client;
    return client.isLinked();
}

bool CloudSaveService::isEnabled() const
{
    return Config::instance().cloudAutoSaveEnabled();
}

bool CloudSaveService::isSyncInProgress() const
{
    return syncInProgress_.load();
}

std::string CloudSaveService::makeBackupFileName() const
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream name;
    name << "retroarch-saves-" << std::put_time(&tm, "%Y-%m-%d-%H%M%S") << ".zip";
    return name.str();
}

std::string CloudSaveService::makePreRestoreZipPath() const
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream path;
    path << paths::CLOUD_DIR << "/pre_restore-" << std::put_time(&tm, "%Y-%m-%d-%H%M%S") << ".zip";
    return path.str();
}

CloudSyncResult CloudSaveService::runSync(CloudProgressCallback progress)
{
    CloudLogSession session("Cloud backup upload");
    CloudSyncResult result;

    if (!Network::waitForConnection(20)) {
        result.message = "No internet connection";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!Network::acquireSession()) {
        result.message = "Network session unavailable";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }

    struct SessionGuard {
        ~SessionGuard() { Network::releaseSession(); }
    } sessionGuard;

    if (progress)
        progress("Preparing…", 5);
    CloudLog::write("Preparing backup upload");

    GoogleDriveClient drive;
    if (!drive.oauthConfigPresent()) {
        result.message = "Google Drive is not configured in this build";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!drive.isLinked()) {
        result.message = "Link Google account in Settings";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!drive.reload()) {
        result.message = drive.lastError().empty() ? "Google Drive not ready" : drive.lastError();
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    CloudLog::write("Google Drive ready");

    if (progress)
        progress("Collecting saves…", 15);

    const RetroArchSaveRoots roots = discoverRetroArchSaveRoots();
    CloudLog::write("Save folder: " + roots.savesDir);
    CloudLog::write("State folder: " + roots.statesDir);

    const std::vector<SavePathEntry> saveEntries = collectAllSavePaths();
    CloudLog::writef("Collected %zu file(s) for backup", saveEntries.size());
    if (saveEntries.empty()) {
        result.message = "No RetroArch save files found";
        result.ok = true;
        CloudLog::write("No save files found — nothing to upload");
        return result;
    }

    std::vector<std::string> saveFiles;
    std::vector<std::string> zipNames;
    saveFiles.reserve(saveEntries.size());
    zipNames.reserve(saveEntries.size());
    for (const auto& entry : saveEntries) {
        saveFiles.push_back(entry.absPath);
        zipNames.push_back(entry.zipPath);
        CloudLog::write("  + " + entry.zipPath + " <- " + entry.absPath);
    }

    if (progress)
        progress("Creating backup…", 35);

    FileSystem::createDirectories(paths::DATA_DIR);
    ZipArchive zip;
    if (!zip.create(paths::CLOUD_TEMP_ZIP, saveFiles, zipNames)) {
        result.message = "Failed to create backup ZIP";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    CloudLog::write("Created local ZIP: " + std::string(paths::CLOUD_TEMP_ZIP));

    result.filesZipped = saveFiles.size();

    if (progress)
        progress("Uploading to Google Drive…", 60);

    const std::string remoteName = makeBackupFileName();
    CloudLog::write("Uploading as: " + remoteName);
    if (!drive.uploadOrPatchFile(paths::CLOUD_TEMP_ZIP, remoteName)) {
        result.message = drive.lastError().empty() ? "Upload failed" : drive.lastError();
        CloudLog::write("ERROR: " + result.message);
        return result;
    }

    FileSystem::removeFile(paths::CLOUD_TEMP_ZIP);
    CloudLog::write("Upload complete, removed temp ZIP");

    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream iso;
    iso << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    Config::instance().setCloudLastSyncIso(iso.str());
    Config::instance().saveUserCloud();

    if (progress)
        progress("Done", 100);

    result.ok = true;
    result.message = "Uploaded " + remoteName + " (" + std::to_string(result.filesZipped) + " files)";
    CloudLog::write("SUCCESS: " + result.message);
    SF_LOG_I("Cloud", "%s", result.message.c_str());
    return result;
}

void CloudSaveService::listBackups(std::function<void(std::vector<CloudBackupInfo>, std::string error)> done)
{
    AppState::instance().pool().enqueue([done = std::move(done)]() mutable {
        CloudLogSession session("Cloud backup list");
        std::vector<CloudBackupInfo> backups;
        std::string error;

        if (!Network::waitForConnection(20)) {
            error = "No internet connection";
        } else if (!Network::acquireSession()) {
            error = "Network session unavailable";
        } else {
            struct SessionGuard {
                ~SessionGuard() { Network::releaseSession(); }
            } sessionGuard;

            GoogleDriveClient drive;
            if (!drive.oauthConfigPresent())
                error = "Google Drive is not configured in this build";
            else if (!drive.isLinked())
                error = "Link Google account in Settings";
            else if (!drive.reload())
                error = drive.lastError().empty() ? "Google Drive not ready" : drive.lastError();
            else {
                std::vector<DriveItem> items;
                if (!drive.listBackupArchives(items))
                    error = drive.lastError().empty() ? "Could not list backups" : drive.lastError();
                else {
                    backups.reserve(items.size());
                    for (const auto& item : items) {
                        CloudBackupInfo info;
                        info.id = item.id;
                        info.name = item.name;
                        info.size = item.size;
                        info.displayLabel = formatBackupLabel(item.name);
                        backups.push_back(std::move(info));
                        CloudLog::writef("  backup: %s (%lld bytes, id=%s)", item.name.c_str(),
                                         static_cast<long long>(item.size), item.id.c_str());
                    }
                    CloudLog::writef("Found %zu backup(s)", backups.size());
                }
            }
        }

        if (!error.empty())
            CloudLog::write("ERROR: " + error);

        brls::sync([done = std::move(done), backups = std::move(backups), error = std::move(error)]() mutable {
            if (done)
                done(std::move(backups), std::move(error));
        });
    });
}

CloudRestoreResult CloudSaveService::runRestore(const std::string& backupId, const std::string& backupName,
                                                CloudRestoreProgressCallback progress,
                                                std::atomic<bool>* abort)
{
    CloudLogSession session("Cloud restore");
    CloudLog::write("Backup: " + backupName + " (id=" + backupId + ")");

    CloudRestoreResult result;
    auto emit = [&](const CloudRestoreProgress& p) {
        if (progress)
            progress(p);
    };

    if (!Network::waitForConnection(20)) {
        result.message = "No internet connection";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!Network::acquireSession()) {
        result.message = "Network session unavailable";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }

    struct SessionGuard {
        ~SessionGuard() { Network::releaseSession(); }
    } sessionGuard;

    const RetroArchSaveRoots roots = discoverRetroArchSaveRoots();
    CloudLog::write("Save folder: " + roots.savesDir);
    CloudLog::write("State folder: " + roots.statesDir);
    emit({CloudRestoreProgress::Kind::Info, "Save folder (retroarch.cfg)",
          roots.savesDir, 0, true, false});
    emit({CloudRestoreProgress::Kind::Info, "State folder (retroarch.cfg)",
          roots.statesDir, 0, true, false});

    GoogleDriveClient drive;
    if (!drive.oauthConfigPresent()) {
        result.message = "Google Drive is not configured in this build";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!drive.isLinked()) {
        result.message = "Link Google account in Settings";
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    if (!drive.reload()) {
        result.message = drive.lastError().empty() ? "Google Drive not ready" : drive.lastError();
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    CloudLog::write("Google Drive ready");

    emit({CloudRestoreProgress::Kind::PreBackup, "Creating local safety backup…", {}, 10, true, false});
    CloudLog::write("Creating pre-restore backup");

    const std::vector<SavePathEntry> saveEntries = collectAllSavePaths();
    CloudLog::writef("Local save files for pre-backup: %zu", saveEntries.size());
    std::vector<std::string> saveFiles;
    std::vector<std::string> zipNames;
    saveFiles.reserve(saveEntries.size());
    zipNames.reserve(saveEntries.size());
    for (const auto& entry : saveEntries) {
        saveFiles.push_back(entry.absPath);
        zipNames.push_back(entry.zipPath);
    }

    FileSystem::createDirectories(paths::CLOUD_DIR);
    result.preRestoreZipPath = makePreRestoreZipPath();
    if (!saveFiles.empty()) {
        ZipArchive preZip;
        if (!preZip.create(result.preRestoreZipPath, saveFiles, zipNames)) {
            result.message = "Failed to create pre-restore backup";
            CloudLog::write("ERROR: " + result.message);
            return result;
        }
        CloudLog::write("Pre-restore ZIP: " + result.preRestoreZipPath);
        emit({CloudRestoreProgress::Kind::PreBackup, "Pre-restore backup saved",
              result.preRestoreZipPath, 20, true, false});
    } else {
        CloudLog::write("No local saves to include in pre-restore backup");
        emit({CloudRestoreProgress::Kind::PreBackup, "No local saves to back up first", {}, 20, true, false});
    }

    if (abort && abort->load()) {
        result.aborted = true;
        result.message = "Restore aborted";
        CloudLog::write("ABORTED before download");
        return result;
    }

    emit({CloudRestoreProgress::Kind::Download, "Downloading " + backupName, {}, 30, true, false});
    CloudLog::write("Downloading to: " + std::string(paths::CLOUD_RESTORE_ZIP));
    if (!drive.downloadFileById(backupId, paths::CLOUD_RESTORE_ZIP)) {
        result.message = drive.lastError().empty() ? "Download failed" : drive.lastError();
        CloudLog::write("ERROR: " + result.message);
        return result;
    }
    CloudLog::write("Download complete");

    if (abort && abort->load()) {
        result.aborted = true;
        result.message = "Restore aborted after download";
        FileSystem::removeFile(paths::CLOUD_RESTORE_ZIP);
        CloudLog::write("ABORTED after download");
        return result;
    }

    emit({CloudRestoreProgress::Kind::Extract, "Restoring files…", {}, 45, true, false});
    CloudLog::write("Extracting and merging files");

    ZipArchive zip;
    const bool extracted = zip.extractMerge(
        paths::CLOUD_RESTORE_ZIP,
        [](const std::string& zipEntry) { return resolveRestoreTarget(zipEntry); },
        [&](const std::string& zipEntry, const std::string& destPath, bool ok, bool skipped) {
            if (skipped) {
                ++result.filesSkipped;
                CloudLog::write("SKIP " + zipEntry);
                emit({CloudRestoreProgress::Kind::Extract, "Skipped (unknown path)", zipEntry, 50, true, true});
                return;
            }
            if (ok) {
                ++result.filesRestored;
                CloudLog::write("OK   " + zipEntry + " -> " + destPath);
                emit({CloudRestoreProgress::Kind::Extract, "Restored", destPath.empty() ? zipEntry : destPath,
                      50, true, false});
            } else {
                ++result.filesFailed;
                CloudLog::write("FAIL " + zipEntry + " -> " + destPath);
                emit({CloudRestoreProgress::Kind::Extract, "Failed", destPath.empty() ? zipEntry : destPath, 50,
                      false, false});
            }
        },
        abort);

    FileSystem::removeFile(paths::CLOUD_RESTORE_ZIP);
    CloudLog::write("Removed downloaded ZIP");

    if (abort && abort->load()) {
        result.aborted = true;
        result.message = "Restore aborted — some files may have been merged";
        CloudLog::write("ABORTED during extract — restored=" + std::to_string(result.filesRestored)
                        + " failed=" + std::to_string(result.filesFailed));
        emit({CloudRestoreProgress::Kind::Summary, result.message, {}, 100, false, false});
        return result;
    }

    if (!extracted && result.filesRestored == 0) {
        result.message = "Failed to extract backup";
        CloudLog::write("ERROR: " + result.message);
        emit({CloudRestoreProgress::Kind::Summary, result.message, {}, 100, false, false});
        return result;
    }

    result.ok = result.filesFailed == 0 && !result.aborted;
    std::ostringstream summary;
    summary << "Restored " << result.filesRestored << " file(s)";
    if (result.filesSkipped > 0)
        summary << ", skipped " << result.filesSkipped;
    if (result.filesFailed > 0)
        summary << ", failed " << result.filesFailed;
    if (!result.preRestoreZipPath.empty() && !saveFiles.empty())
        summary << ". Pre-restore backup: " << result.preRestoreZipPath;
    result.message = summary.str();

    CloudLog::write(result.ok ? "SUCCESS: " + result.message : "DONE WITH ERRORS: " + result.message);
    emit({CloudRestoreProgress::Kind::Summary, result.message, {}, 100, result.ok, false});
    SF_LOG_I("Cloud", "Restore %s", result.message.c_str());
    return result;
}

void CloudSaveService::restoreNow(const std::string& backupId, const std::string& backupName,
                                  CloudRestoreProgressCallback progress, std::atomic<bool>* abort,
                                  std::function<void(CloudRestoreResult)> done)
{
    AppState::instance().pool().enqueue(
        [backupId, backupName, progress = std::move(progress), abort, done = std::move(done)]() mutable {
            CloudRestoreProgressCallback mainProgress;
            if (progress) {
                mainProgress = [progress = std::move(progress)](const CloudRestoreProgress& p) {
                    brls::sync([progress, p]() { progress(p); });
                };
            }
            CloudRestoreResult result =
                instance().runRestore(backupId, backupName, mainProgress, abort);
            brls::sync([done = std::move(done), result]() mutable {
                if (done)
                    done(result);
            });
        });
}

void CloudSaveService::syncNow(CloudProgressCallback progress,
                               std::function<void(CloudSyncResult)> done)
{
    bool expected = false;
    if (!syncInProgress_.compare_exchange_strong(expected, true)) {
        CloudSyncResult skipped;
        skipped.message = "Backup already in progress";
        SF_LOG_I("Cloud", "Sync skipped — %s", skipped.message.c_str());
        brls::sync([done = std::move(done), skipped]() mutable {
            if (done)
                done(skipped);
        });
        return;
    }

    AppState::instance().pool().enqueue([progress = std::move(progress),
                                         done = std::move(done)]() mutable {
        CloudSyncResult result = instance().runSync(progress);
        syncInProgress_.store(false);
        brls::sync([done = std::move(done), result]() mutable {
            if (done)
                done(result);
        });
    });
}

void CloudSaveService::maybeAutoSyncAfterReturn()
{
    if (!isEnabled() || !isLinked())
        return;

    if (syncInProgress_.load()) {
        SF_LOG_I("Cloud", "Auto cloud backup skipped — backup already in progress");
        return;
    }

    if (!Network::isAvailable()) {
        SF_LOG_I("Cloud", "Auto cloud backup skipped — no network");
        return;
    }

    if (collectAllSavePaths().empty()) {
        SF_LOG_I("Cloud", "Auto cloud backup skipped — no save files");
        return;
    }

    SF_LOG_I("Cloud", "Auto cloud backup after return from game");
    syncNow(nullptr, [](CloudSyncResult result) {
        if (result.ok && result.filesZipped > 0)
            brls::Application::notify("Cloud backup: " + result.message);
        else if (!result.ok)
            SF_LOG_W("Cloud", "Auto cloud backup failed: %s", result.message.c_str());
    });
}

} // namespace sf::cloud
