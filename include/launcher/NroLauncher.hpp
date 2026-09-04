#pragma once

#include "app/Models.hpp"

#include <string>

namespace sf {

class NroLauncher {
public:
    /** Launch a game through RetroArch (core + ROM). Opens RA in-game menu via hotkey. */
    static bool launch(const SystemConfig& system, const std::string& romPath, std::string& errorOut);

    /** Open the RetroArch frontend menu (core downloader, settings, etc.). */
    static bool openRetroArchMenu(std::string& errorOut);

    static bool fileExists(const std::string& path);
};

} // namespace sf
