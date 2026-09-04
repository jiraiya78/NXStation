#pragma once

#include <functional>
#include <string>

namespace sf {

/** Check GitHub Releases and optionally download NXStation.nro over the running install. */
class AppUpdater {
public:
    struct CheckResult {
        bool ok = false;
        bool updateAvailable = false;
        bool upToDate = false;
        std::string remoteVersion;   // e.g. "0.1.8"
        std::string releaseName;     // GitHub release name/tag
        std::string downloadUrl;     // browser_download_url for NXStation.nro
        std::string error;
        std::string message;         // user-facing summary
    };

    enum class BootstrapResult {
        Continue,          // no pending update, or installed — keep starting
        ExitForRelaunch,   // envSetNextLoad already set; main must return immediately
    };

    using CheckCallback = std::function<void(CheckResult)>;
    using DownloadCallback = std::function<void(bool ok, std::string message)>;
    using ProgressCallback = std::function<void(int percent)>;

    static constexpr const char* kGitHubOwner = "jiraiya78";
    static constexpr const char* kGitHubRepo = "NXStation";
    static constexpr const char* kAssetName = "NXStation.nro";

    /** Query GitHub Releases API, then fall back to raw version.json on the main branch. */
    static void checkForUpdate(CheckCallback cb);

    static void tryVersionJsonFallback(CheckCallback cb);

    /**
     * Download release to NXStation_update.nro (staging). Does not overwrite the locked
     * running NRO — relaunchViaNextLoad() must load the update NRO next.
     */
    static void downloadAndInstall(const std::string& downloadUrl, DownloadCallback cb,
                                   ProgressCallback progress = {});

    /**
     * Call at process start (before Borealis).
     * - If update NRO exists and APP_NRO is writable: copy install, continue.
     * - If update NRO exists but APP_NRO is locked: envSetNextLoad(update) and
     *   return ExitForRelaunch so main exits and the loader runs the new build.
     */
    static BootstrapResult bootstrapApply(int argc, char* argv[]);

    /** Path to load after a successful download (update NRO if present). */
    static std::string pendingOrInstalledPath();

    /** envSetNextLoad + quit so the loader runs the new NRO (Borealis must be up). */
    static bool relaunchViaNextLoad(const std::string& nroPath, std::string& errorOut);

    /** Compare "0.1.7" / "v0.1.8" style versions. Returns <0, 0, >0. */
    static int compareVersions(std::string a, std::string b);
};

} // namespace sf
