#include "launcher/NroLauncher.hpp"
#include "launcher/ReturnChain.hpp"
#include "analytics/PlaySessionTracker.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "util/ActionLog.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#include <borealis.hpp>

#include <string>

#ifdef __SWITCH__
#include <switch.h>
#include <unistd.h>
#endif

namespace sf {

namespace {

constexpr const char* kSalamanderCfg = "sdmc:/retroarch/retroarch-salamander.cfg";

bool needsQuoting(const std::string& path)
{
    return path.find_first_of(" \t(\"')") != std::string::npos;
}

std::string quoteIfNeeded(const std::string& path)
{
    if (!needsQuoting(path))
        return path;
    return "\"" + path + "\"";
}

/**
 * RetroArch Switch reads libretro_path from salamander.cfg at boot for the
 * on-screen core banner / info. A stale value (e.g. Dolphin after a GC launch)
 * makes every subsequent core show "Nintendo Wii/GameCube" even when argv[0]
 * is the correct core — and zip/7z associations can follow that stale path.
 * Rewrite it to the core we are about to chain-load.
 */
void syncSalamanderCorePath(const std::string& corePath)
{
    if (corePath.empty())
        return;

    FileSystem::createDirectories("sdmc:/retroarch");
    const std::string body = std::string("libretro_path = \"") + corePath + "\"\n";
    if (FileSystem::writeFile(kSalamanderCfg, body))
        SF_LOG_I("Launch", "Updated salamander cfg → %s", corePath.c_str());
    else
        SF_LOG_W("Launch", "Failed to write %s", kSalamanderCfg);
}

bool chainLoad(const std::string& targetNro, const std::string& args, std::string& errorOut)
{
    if (!FileSystem::exists(targetNro)) {
        errorOut = "Not found:\n" + targetNro;
        SF_LOG_E("Launch", "%s", errorOut.c_str());
        return false;
    }

    SF_LOG_I("Launch", "envSetNextLoad target=%s", targetNro.c_str());
    SF_LOG_I("Launch", "args=%s", args.c_str());

    writeReturnChainMarker();

#ifdef __SWITCH__
    if (chdir("sdmc:/retroarch") != 0)
        SF_LOG_W("Launch", "chdir(sdmc:/retroarch) failed — continuing");

    Result rc = envSetNextLoad(targetNro.c_str(), args.c_str());
    if (R_FAILED(rc)) {
        errorOut = "envSetNextLoad failed (0x" + std::to_string(static_cast<unsigned>(rc)) + ")";
        SF_LOG_E("Launch", "%s", errorOut.c_str());
        return false;
    }
#else
    SF_LOG_I("Launch", "Desktop stub — would launch: %s %s", targetNro.c_str(), args.c_str());
#endif

    Logger::instance().flush();
    AppState::instance().beginHandoff();
    brls::Application::quit();
    return true;
}

// RetroArch Switch forks to the core NRO with: argv[0]=core, argv[1]=rom (see platform_switch.c).
std::string buildCoreDirectArgs(const std::string& core, const std::string& romPath)
{
    std::string args = core;
    if (!romPath.empty()) {
        args += ' ';
        args += quoteIfNeeded(romPath);
    }
    return args;
}

} // namespace

bool NroLauncher::fileExists(const std::string& path)
{
    return FileSystem::exists(path);
}

bool NroLauncher::launch(const SystemConfig& system, const std::string& romPath, std::string& errorOut)
{
    SF_LOG_ACTION("Launch/Game");

    if (AppState::instance().isAppletMode()) {
        errorOut = "Game launch needs Title Override.\nApplet Mode does not have enough RAM.";
        SF_LOG_W("Launch", "%s", errorOut.c_str());
        return false;
    }

    if (!fileExists(romPath)) {
        errorOut = "ROM not found:\n" + romPath;
        SF_LOG_E("Launch", "%s", errorOut.c_str());
        return false;
    }

    const std::string core = Config::instance().coreFor(system.id);
    if (core.empty()) {
        errorOut = "No core configured for: " + system.name;
        return false;
    }

    if (!fileExists(core)) {
        errorOut = "Core not found:\n" + core
                   + "\n\nSet the core path in Settings → Core Paths";
        SF_LOG_E("Launch", "%s", errorOut.c_str());
        return false;
    }

    SF_LOG_I("Launch", "ROM=%s", romPath.c_str());
    SF_LOG_I("Launch", "Core=%s", core.c_str());

    sf::analytics::PlaySessionTracker::beginSession(system.id, romPath,
                                                    FileSystem::stemOf(romPath), core);

    syncSalamanderCorePath(core);

    // Core-direct is what RetroArch itself uses on Switch (envSetNextLoad to core.nro).
    return chainLoad(core, buildCoreDirectArgs(core, romPath), errorOut);
}

bool NroLauncher::openRetroArchMenu(std::string& errorOut)
{
    SF_LOG_ACTION("Launch/RetroArchMenu");

    const std::string& retroArch = Config::instance().retroArchPath();
    if (!fileExists(retroArch)) {
        errorOut = "RetroArch not found:\n" + retroArch;
        return false;
    }

    return chainLoad(retroArch, retroArch + " --menu", errorOut);
}

} // namespace sf
