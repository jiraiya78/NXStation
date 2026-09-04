#pragma once

#include "app/Models.hpp"

#include "scraper/ScrapeTypes.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace sf {

using ScrapeCallback = std::function<void(bool success, GameMetadata meta, std::string error)>;

class ScraperService {
public:
    ScraperService();
    ~ScraperService();

    /** Queue a scrape job (hash + API). Completes via brls::sync on main thread. */
    void scrapeAsync(const GameItem& game, const SystemConfig& system, ScrapeCallback cb);

    /** Run a batch on a worker thread; progress/done callbacks are on the main thread. */
    void runBatchAsync(std::vector<GameItem> games, SystemConfig system, ScrapeMode mode,
                       ScrapeProgressCb onProgress, ScrapeBatchDoneCb onDone);

    void requestAbort();
    void clearAbort();
    bool isAbortRequested() const { return abortRequested_; }

    void setRateLimitMs(size_t ms) { rateLimitMs_ = ms; }
    void cancelInflight();

private:
    void releaseInFlight(const std::string& path);
    bool acquireSlot();
    void releaseSlot();

    GameMetadata scrapeBlocking(const GameItem& game, const SystemConfig& system, ScrapeMode mode,
                                std::string& error,
                                ScrapeProgressCb stepProgress = nullptr);
    std::string buildQueryUrl(const GameItem& game, const SystemConfig& system, const std::string& crc);
    bool parseScreenScraperJson(const std::string& json, GameMetadata& out, std::string& error,
                                std::vector<std::string>* manualUrlsOut = nullptr);
    bool downloadAsset(const std::string& url, const std::string& dest, bool preserveFormat = false);

    size_t rateLimitMs_ = 350;

    std::mutex inFlightMutex_;
    std::unordered_set<std::string> inFlightPaths_;

    std::mutex slotMutex_;
    std::condition_variable slotCv_;
    bool slotBusy_ = false;

    std::atomic<bool> abortRequested_{false};
};

} // namespace sf
