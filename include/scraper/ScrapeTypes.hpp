#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace sf {

enum class ScrapeMode {
    Full,
    MissingArtOnly,
    Single,
};

struct ScrapeProgress {
    size_t index = 0;
    size_t total = 0;
    size_t succeeded = 0;
    size_t failed = 0;
    size_t skipped = 0;
    std::string gameName;
    std::string phase;
    std::string detail;
    /** Set for per-asset rows (box art / snapshot / video). */
    bool hasResult = false;
    bool success = false;
};

using ScrapeProgressCb = std::function<void(ScrapeProgress)>;
using ScrapeBatchDoneCb = std::function<void(size_t succeeded, size_t failed, size_t skipped, bool aborted)>;

struct ScrapeAssetOptions {
    bool boxArt = true;
    bool thumbnail = true;
    bool video = true;
};

} // namespace sf
