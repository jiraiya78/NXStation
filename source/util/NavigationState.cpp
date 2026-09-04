#include "util/NavigationState.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

namespace sf {

namespace {

SavedNavigation g_current;
SavedNavigation g_pendingRestore;
std::string g_lastSystem;
bool g_restoreLoaded = false;

void loadRestoreFromDisk()
{
    if (g_restoreLoaded)
        return;
    g_restoreLoaded = true;

    if (!FileSystem::exists(paths::NAV_STATE_PATH))
        return;

    try {
        const std::string text = FileSystem::readFile(paths::NAV_STATE_PATH);
        if (text.empty())
            return;

        const Json root = Json::parse(text);
        g_lastSystem = root.value("lastSystem", std::string());

        if (!root.value("restore", false))
            return;

        g_pendingRestore.shouldRestore = true;
        g_pendingRestore.systemId = root.value("systemId", std::string());
        g_pendingRestore.gameIndex = root.value("gameIndex", size_t(0));
        g_pendingRestore.romPath = root.value("romPath", std::string());

        SF_LOG_I("Nav", "Pending restore: system=%s index=%zu rom=%s",
                 g_pendingRestore.systemId.c_str(), g_pendingRestore.gameIndex,
                 g_pendingRestore.romPath.c_str());
    } catch (const std::exception& ex) {
        SF_LOG_W("Nav", "Could not parse navigation state: %s", ex.what());
    }
}

void writeState(bool restoreFlag)
{
    Json root = Json::object();
    root["restore"] = restoreFlag;
    root["systemId"] = g_current.systemId;
    root["gameIndex"] = static_cast<double>(g_current.gameIndex);
    root["romPath"] = g_current.romPath;
    root["lastSystem"] = g_lastSystem;

    if (!FileSystem::writeFile(paths::NAV_STATE_PATH, root.dump()))
        SF_LOG_W("Nav", "Failed to write navigation state");
}

} // namespace

void NavigationState::update(const std::string& systemId, size_t gameIndex,
                             const std::string& romPath)
{
    g_current.systemId = systemId;
    g_current.gameIndex = gameIndex;
    g_current.romPath = romPath;
}

void NavigationState::persistForHandoff()
{
    if (g_current.systemId.empty())
        return;

    SF_LOG_I("Nav", "Persisting navigation for handoff: system=%s index=%zu",
             g_current.systemId.c_str(), g_current.gameIndex);
    writeState(true);
}

SavedNavigation NavigationState::takePendingRestore()
{
    loadRestoreFromDisk();

    SavedNavigation out = g_pendingRestore;
    g_pendingRestore = {};

    if (out.shouldRestore)
        writeState(false);

    return out;
}

void NavigationState::setLastSystem(const std::string& systemId)
{
    loadRestoreFromDisk();

    if (systemId.empty() || systemId == g_lastSystem)
        return;

    g_lastSystem = systemId;
    writeState(false);
}

std::string NavigationState::lastSystem()
{
    loadRestoreFromDisk();
    return g_lastSystem;
}

} // namespace sf
