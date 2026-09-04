#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sf::cloud {

using ZipTargetMapper = std::function<std::optional<std::string>(const std::string& zipEntry)>;
using ZipExtractProgress =
    std::function<void(const std::string& zipEntry, const std::string& destPath, bool ok, bool skipped)>;

/** Create a ZIP archive from local files (Switch / minizip). */
class ZipArchive {
public:
    bool create(const std::string& zipPath, const std::vector<std::string>& files,
                const std::vector<std::string>& zipNames);

    /** Extract ZIP entries, merging into mapped destinations (overwrite only). */
    bool extractMerge(const std::string& zipPath, ZipTargetMapper mapper, ZipExtractProgress progress,
                      std::atomic<bool>* abort);
};

} // namespace sf::cloud
