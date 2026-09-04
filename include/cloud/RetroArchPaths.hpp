#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sf::cloud {

/** Discover RetroArch savefile / savestate directories on SD. */
struct RetroArchSaveRoots {
    std::string savesDir;
    std::string statesDir;
};

/** A file to include in a cloud backup ZIP. */
struct SavePathEntry {
    std::string absPath;
    std::string zipPath;
};

RetroArchSaveRoots discoverRetroArchSaveRoots();

/** Collect RetroArch central saves/states plus ROM-folder sidecar saves. */
std::vector<SavePathEntry> collectAllSavePaths();

/** Map a ZIP entry path to the local restore destination (current RetroArch paths). */
std::optional<std::string> resolveRestoreTarget(const std::string& zipEntryPath);

} // namespace sf::cloud