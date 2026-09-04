#include "util/AppUpdater.hpp"

#include "app/AppState.hpp"
#include "app/Version.hpp"
#include "scraper/HttpClient.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Network.hpp"
#include "util/Paths.hpp"

#include <borealis.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf {

namespace {

std::string stripVersionPrefix(std::string v)
{
    while (!v.empty() && (v.front() == 'v' || v.front() == 'V'))
        v.erase(v.begin());
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())))
        v.erase(v.begin());
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))
        v.pop_back();
    return v;
}

std::vector<int> parseVersionParts(const std::string& v)
{
    std::vector<int> parts;
    std::stringstream ss(stripVersionPrefix(v));
    std::string piece;
    while (std::getline(ss, piece, '.')) {
        int n = 0;
        for (char c : piece) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                break;
            n = n * 10 + (c - '0');
        }
        parts.push_back(n);
    }
    while (parts.size() < 3)
        parts.push_back(0);
    return parts;
}

std::string latestApiUrl()
{
    return std::string("https://api.github.com/repos/") + AppUpdater::kGitHubOwner + "/" +
           AppUpdater::kGitHubRepo + "/releases/latest";
}

std::string tmpDownloadPath()
{
    return std::string(paths::APP_ROOT) + "/NXStation_update.nro.tmp";
}

/** Must end in .nro so hbloader accepts it via envSetNextLoad. */
std::string updateNroPath()
{
    return std::string(paths::APP_ROOT) + "/NXStation_update.nro";
}

/** Legacy staging name from earlier builds — migrate if present. */
std::string legacyPendingPath()
{
    return std::string(paths::APP_NRO) + ".pending";
}

uint64_t fileSizeBytes(const std::string& path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) != 0)
        return 0;
    return static_cast<uint64_t>(st.st_size);
}

constexpr uint64_t kMinNroBytes = 512 * 1024;

#define UPDATER_LOG_I(...)                                                                           \
    do {                                                                                             \
        SF_LOG_I("Updater", __VA_ARGS__);                                                            \
        Logger::instance().flush();                                                                  \
    } while (0)
#define UPDATER_LOG_W(...)                                                                           \
    do {                                                                                             \
        SF_LOG_W("Updater", __VA_ARGS__);                                                            \
        Logger::instance().flush();                                                                  \
    } while (0)
#define UPDATER_LOG_E(...)                                                                           \
    do {                                                                                             \
        SF_LOG_E("Updater", __VA_ARGS__);                                                            \
        Logger::instance().flush();                                                                  \
    } while (0)

std::string latestDownloadUrl()
{
    return std::string("https://github.com/") + AppUpdater::kGitHubOwner + "/" +
           AppUpdater::kGitHubRepo + "/releases/latest/download/" + AppUpdater::kAssetName;
}

std::string versionJsonUrl()
{
    return std::string("https://raw.githubusercontent.com/") + AppUpdater::kGitHubOwner + "/" +
           AppUpdater::kGitHubRepo + "/main/version.json";
}

bool looksLikeNro(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    // NroHeader.magic sits at offset 16 (after NroStart).
    in.seekg(16);
    char magic[4]{};
    in.read(magic, 4);
    return static_cast<bool>(in) && std::memcmp(magic, "NRO0", 4) == 0;
}

void migrateLegacyPending()
{
    const std::string legacy = legacyPendingPath();
    const std::string update = updateNroPath();
    if (!FileSystem::exists(legacy))
        return;
    if (FileSystem::exists(update)) {
        FileSystem::removeFile(legacy);
        return;
    }
    UPDATER_LOG_I("Migrating legacy pending → %s", update.c_str());
    if (!FileSystem::renameFile(legacy, update)) {
        if (FileSystem::copyFile(legacy, update))
            FileSystem::removeFile(legacy);
    }
}

bool parseCheckJson(const std::string& body, AppUpdater::CheckResult& r)
{
    try {
        const auto j = Json::parse(body);
        const std::string tag = j.value("tag_name", std::string());
        const std::string name = j.value("name", tag);
        r.remoteVersion = stripVersionPrefix(tag);
        r.releaseName = name.empty() ? tag : name;

        if (r.remoteVersion.empty()) {
            r.error = "Missing tag_name";
            r.message = "Unexpected GitHub response";
            return false;
        }

        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                if (!asset.is_object())
                    continue;
                if (asset.value("name", std::string()) != AppUpdater::kAssetName)
                    continue;
                r.downloadUrl = asset.value("browser_download_url", std::string());
                break;
            }
        }
        return true;
    } catch (const std::exception& ex) {
        r.error = ex.what();
        r.message = "Failed to parse GitHub release JSON";
        return false;
    }
}

bool parseVersionJson(const std::string& body, AppUpdater::CheckResult& r)
{
    try {
        const auto j = Json::parse(body);
        r.remoteVersion = stripVersionPrefix(j.value("version", std::string()));
        r.downloadUrl = j.value("download", latestDownloadUrl());
        r.releaseName = "v" + r.remoteVersion;
        if (r.remoteVersion.empty()) {
            r.error = "Missing version";
            r.message = "version.json has no version field";
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        r.error = ex.what();
        r.message = "Failed to parse version.json";
        return false;
    }
}

void finalizeCheckResult(AppUpdater::CheckResult& r)
{
    const int cmp = AppUpdater::compareVersions(kAppVersion, r.remoteVersion);
    r.ok = true;
    if (cmp >= 0) {
        r.upToDate = true;
        r.message = std::string("You're on the latest version (v") + kAppVersion + ")";
    } else if (r.downloadUrl.empty()) {
        r.updateAvailable = true;
        r.downloadUrl = latestDownloadUrl();
        if (r.downloadUrl.empty()) {
            r.error = "No download URL";
            r.message = "Update v" + r.remoteVersion + " found but no download URL";
        } else {
            r.updateAvailable = true;
            r.message = std::string("Update available: v") + kAppVersion + " → v" + r.remoteVersion;
        }
    } else {
        r.updateAvailable = true;
        r.message = std::string("Update available: v") + kAppVersion + " → v" + r.remoteVersion;
    }
}

/**
 * Install staged NRO into APP_NRO.
 * Prefer native FS overwrite (Sphaira pattern) — works while the running NRO is locked
 * against POSIX rename/unlink/ofstream trunc (errno 5).
 */
bool installPendingToAppNro(const std::string& pending)
{
    const uint64_t pendingBytes = fileSizeBytes(pending);
    if (pendingBytes < kMinNroBytes || !looksLikeNro(pending)) {
        UPDATER_LOG_E("Update file invalid (%llu bytes, nro=%d)",
                      static_cast<unsigned long long>(pendingBytes), looksLikeNro(pending) ? 1 : 0);
        return false;
    }

    UPDATER_LOG_I("Installing %s (%llu bytes) → %s (native overwrite)", pending.c_str(),
                  static_cast<unsigned long long>(pendingBytes), paths::APP_NRO);

    if (!FileSystem::overwriteFile(pending, paths::APP_NRO)) {
        UPDATER_LOG_E("overwriteFile failed (errno %d)", errno);
        return false;
    }

    const uint64_t installed = fileSizeBytes(paths::APP_NRO);
    if (installed != pendingBytes) {
        UPDATER_LOG_E("Install size mismatch (%llu vs %llu)",
                      static_cast<unsigned long long>(installed),
                      static_cast<unsigned long long>(pendingBytes));
        return false;
    }

    UPDATER_LOG_I("Installed update into APP_NRO (%llu bytes)",
                  static_cast<unsigned long long>(installed));
    FileSystem::removeFile(pending);
    FileSystem::removeFile(legacyPendingPath());
    return true;
}

bool setNextLoadOnly(const std::string& nroPath, std::string& errorOut)
{
    if (!FileSystem::exists(nroPath)) {
        errorOut = "Update file missing:\n" + nroPath;
        return false;
    }

#ifdef __SWITCH__
    if (!envHasNextLoad()) {
        errorOut = "This launch mode cannot chain-load the update.\n"
                   "Launch NXStation from hbmenu (not applet), then try again.\n\n"
                   "Or manually replace NXStation.nro with NXStation_update.nro";
        UPDATER_LOG_E("envHasNextLoad() == false");
        return false;
    }

    // argv must start with the NRO path (hbloader convention).
    Result rc = envSetNextLoad(nroPath.c_str(), nroPath.c_str());
    if (R_FAILED(rc)) {
        errorOut = "envSetNextLoad failed (0x" + std::to_string(static_cast<unsigned>(rc)) + ")";
        UPDATER_LOG_E("%s", errorOut.c_str());
        return false;
    }
    UPDATER_LOG_I("envSetNextLoad OK → %s (envHasNextLoad=1)", nroPath.c_str());
#else
    (void)errorOut;
    UPDATER_LOG_I("Desktop stub — next load %s", nroPath.c_str());
#endif
    Logger::instance().flush();
    return true;
}

} // namespace

int AppUpdater::compareVersions(std::string a, std::string b)
{
    const auto pa = parseVersionParts(std::move(a));
    const auto pb = parseVersionParts(std::move(b));
    const size_t n = std::max(pa.size(), pb.size());
    for (size_t i = 0; i < n; ++i) {
        const int av = i < pa.size() ? pa[i] : 0;
        const int bv = i < pb.size() ? pb[i] : 0;
        if (av < bv)
            return -1;
        if (av > bv)
            return 1;
    }
    return 0;
}

std::string AppUpdater::pendingOrInstalledPath()
{
    migrateLegacyPending();
    const std::string update = updateNroPath();
    // If staging remains, APP_NRO was not overwritten — relaunch the staged build.
    if (FileSystem::exists(update) && fileSizeBytes(update) >= kMinNroBytes)
        return update;
    return paths::APP_NRO;
}

AppUpdater::BootstrapResult AppUpdater::bootstrapApply(int argc, char* argv[])
{
    const char* argv0 = (argc > 0 && argv && argv[0]) ? argv[0] : nullptr;
    migrateLegacyPending();

    const std::string update = updateNroPath();
    if (!FileSystem::exists(update)) {
        UPDATER_LOG_I("No pending update file");
        return BootstrapResult::Continue;
    }

    UPDATER_LOG_I("Pending update present: %s (%llu bytes), argv0=%s", update.c_str(),
                  static_cast<unsigned long long>(fileSizeBytes(update)),
                  argv0 ? argv0 : "(null)");

    // Already applied (sizes match) — clean staging.
    if (FileSystem::exists(paths::APP_NRO) && looksLikeNro(paths::APP_NRO) &&
        fileSizeBytes(paths::APP_NRO) == fileSizeBytes(update)) {
        UPDATER_LOG_I("APP_NRO already matches staged update — removing staging file");
        FileSystem::removeFile(update);
        FileSystem::removeFile(legacyPendingPath());
        return BootstrapResult::Continue;
    }

    if (installPendingToAppNro(update)) {
        UPDATER_LOG_I("Pending update installed into APP_NRO at bootstrap");
        return BootstrapResult::Continue;
    }

    // APP_NRO still locked (e.g. romfs held it open) — chain-load the staged update NRO.
    std::string err;
    if (setNextLoadOnly(update, err)) {
        UPDATER_LOG_I("Bootstrap install failed — exiting to relaunch staged update NRO");
        return BootstrapResult::ExitForRelaunch;
    }
    UPDATER_LOG_W("Bootstrap install failed and relaunch unavailable: %s", err.c_str());
    return BootstrapResult::Continue;
}

bool AppUpdater::relaunchViaNextLoad(const std::string& nroPath, std::string& errorOut)
{
    if (!setNextLoadOnly(nroPath, errorOut))
        return false;

    AppState::instance().beginHandoff();
    brls::Application::quit();
    return true;
}

void AppUpdater::checkForUpdate(CheckCallback cb)
{
    if (!cb)
        return;

    if (!Network::waitForConnection(15)) {
        CheckResult r;
        r.error = "No network connection";
        r.message = "Connect to the internet and try again";
        cb(std::move(r));
        return;
    }

    auto& http = HttpClient::instance();
    http.setConnectTimeout(20);
    http.setTransferTimeout(45);

    const std::string url = latestApiUrl();
    UPDATER_LOG_I("Checking %s (local v%s)", url.c_str(), kAppVersion);

    http.get(url,
             [cb = std::move(cb)](HttpResponse resp) mutable {
                 Network::releaseSession();

                 CheckResult r;
                 if (!resp.ok) {
                     UPDATER_LOG_W("API check failed: %s — trying version.json fallback",
                                   resp.error.c_str());
                     AppUpdater::tryVersionJsonFallback(std::move(cb));
                     return;
                 }

                 if (!parseCheckJson(resp.body, r)) {
                     UPDATER_LOG_W("%s", r.message.c_str());
                     AppUpdater::tryVersionJsonFallback(std::move(cb));
                     return;
                 }

                 finalizeCheckResult(r);
                 UPDATER_LOG_I("%s", r.message.c_str());
                 cb(std::move(r));
             },
             {{"Accept", "application/vnd.github+json"},
              {"X-GitHub-Api-Version", "2022-11-28"}});
}

void AppUpdater::tryVersionJsonFallback(CheckCallback cb)
{
    if (!cb)
        return;

    if (!Network::waitForConnection(10)) {
        CheckResult r;
        r.error = "No network connection";
        r.message = "Update check failed — connect to the internet and try again";
        cb(std::move(r));
        return;
    }

    auto& http = HttpClient::instance();
    const std::string url = versionJsonUrl();
    UPDATER_LOG_I("Fallback check %s", url.c_str());

    http.get(url,
             [cb = std::move(cb)](HttpResponse resp) mutable {
                 Network::releaseSession();

                 CheckResult r;
                 if (!resp.ok) {
                     r.error = resp.error.empty() ? "Request failed" : resp.error;
                     r.message = "Update check failed: " + r.error;
                     UPDATER_LOG_W("%s", r.message.c_str());
                     cb(std::move(r));
                     return;
                 }

                 if (!parseVersionJson(resp.body, r)) {
                     UPDATER_LOG_W("%s: %s", r.message.c_str(), r.error.c_str());
                     cb(std::move(r));
                     return;
                 }

                 finalizeCheckResult(r);
                 UPDATER_LOG_I("%s (via version.json)", r.message.c_str());
                 cb(std::move(r));
             });
}

void AppUpdater::downloadAndInstall(const std::string& downloadUrl, DownloadCallback cb,
                                    ProgressCallback progress)
{
    if (!cb)
        return;

    if (downloadUrl.empty()) {
        UPDATER_LOG_W("Download rejected: missing URL");
        cb(false, "Missing download URL");
        return;
    }

    if (!Network::waitForConnection(15)) {
        UPDATER_LOG_W("Download aborted: no network connection");
        cb(false, "No network connection");
        return;
    }

    const std::string tmp = tmpDownloadPath();
    const std::string update = updateNroPath();
    FileSystem::removeFile(tmp);

    auto& http = HttpClient::instance();
    http.setConnectTimeout(30);
    http.setTransferTimeout(600);

    UPDATER_LOG_I("Downloading %s → %s", downloadUrl.c_str(), update.c_str());

    HttpProgressCallback httpProgress;
    if (progress) {
        httpProgress = [progress = std::move(progress)](int percent) { progress(percent); };
    }

    http.getDownload(
        downloadUrl, tmp,
        [cb = std::move(cb), tmp, update](HttpResponse resp) mutable {
            Network::releaseSession();

            if (!resp.ok) {
                FileSystem::removeFile(tmp);
                const std::string msg =
                    "Download failed: " + (resp.error.empty() ? "unknown error" : resp.error);
                UPDATER_LOG_W("%s (HTTP %ld)", msg.c_str(), resp.status);
                cb(false, msg);
                return;
            }

            if (!FileSystem::exists(tmp)) {
                UPDATER_LOG_E("Download finished but temp file missing: %s", tmp.c_str());
                cb(false, "Download finished but temp file missing");
                return;
            }

            const uint64_t tmpBytes = fileSizeBytes(tmp);
            UPDATER_LOG_I("Download complete: %llu bytes in %s (HTTP %ld)",
                          static_cast<unsigned long long>(tmpBytes), tmp.c_str(), resp.status);

            if (tmpBytes < kMinNroBytes || !looksLikeNro(tmp)) {
                FileSystem::removeFile(tmp);
                UPDATER_LOG_E("Download invalid (bytes=%llu nro_magic=%d)",
                              static_cast<unsigned long long>(tmpBytes), looksLikeNro(tmp) ? 1 : 0);
                cb(false, "Download is not a valid NXStation.nro");
                return;
            }

            FileSystem::removeFile(update);
            FileSystem::removeFile(legacyPendingPath());
            if (!FileSystem::renameFile(tmp, update)) {
                if (!FileSystem::copyFile(tmp, update)) {
                    FileSystem::removeFile(tmp);
                    UPDATER_LOG_E("Could not stage update at %s (errno %d)", update.c_str(), errno);
                    cb(false, "Could not save downloaded update");
                    return;
                }
                FileSystem::removeFile(tmp);
            }

            UPDATER_LOG_I("Update staged at %s", update.c_str());

            // Overwrite running NRO on disk (romfsExit inside overwriteFile clears TargetLocked),
            // then relaunch the same path — Sphaira ExitRestart pattern.
            if (installPendingToAppNro(update)) {
                UPDATER_LOG_I("APP_NRO overwritten — ready to relaunch same path");
                cb(true, "Update ready");
                return;
            }

            // Still staged: relaunch the update NRO so the user gets the new build this session.
            UPDATER_LOG_W("Could not overwrite APP_NRO (TargetLocked?) — will relaunch %s",
                          update.c_str());
            cb(true, "Update ready (relaunch from staged NRO)");
        },
        std::move(httpProgress));
}

} // namespace sf
