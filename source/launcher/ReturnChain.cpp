#include "launcher/ReturnChain.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#ifdef __SWITCH__
#include "forwarder/ForwarderNca.hpp"
#include "forwarder/NroMeta.hpp"

#include <cstdio>
#endif

namespace sf {

namespace {

constexpr const char* HBMENU_PATH = "sdmc:/hbmenu.nro";
constexpr const char* HBMENU_BACKUP = "sdmc:/switch/NXStation/backup/hbmenu.nro";
constexpr const char* HBMENU_MARKER = "sdmc:/switch/NXStation/settings/hbmenu_return.json";

} // namespace

void writeReturnChainMarker()
{
    std::string payload = std::string(paths::APP_NRO) + "\n";
#ifdef __SWITCH__
    const u64 tid = forwarderTitleId(nroArgvPath(paths::APP_NRO), "");
    char line[32];
    std::snprintf(line, sizeof(line), "%016llX\n",
                  static_cast<unsigned long long>(tid));
    payload += line;
    SF_LOG_I("Launch", "Return chain marker tid=%016llX",
             static_cast<unsigned long long>(tid));
#endif
    if (!FileSystem::writeFile(paths::RETURN_CHAIN_PATH, payload))
        SF_LOG_W("Launch", "Could not write return chain marker");
}

bool hbmenuWasReplaced()
{
    return FileSystem::exists(HBMENU_MARKER);
}

bool restoreHbmenuBackup(std::string& errorOut)
{
    if (!FileSystem::exists(HBMENU_MARKER))
        return false;

    bool restored = false;
    if (FileSystem::exists(HBMENU_BACKUP)) {
        const std::string data = FileSystem::readFile(HBMENU_BACKUP);
        if (data.empty()) {
            errorOut = "Could not read hbmenu backup";
        } else if (!FileSystem::writeFile(HBMENU_PATH, data)) {
            errorOut = "Could not write sdmc:/hbmenu.nro";
        } else {
            restored = true;
            SF_LOG_I("Launch", "Restored original hbmenu.nro from backup");
        }
    } else {
        errorOut = "No hbmenu backup found";
    }

    FileSystem::removeFile(HBMENU_MARKER);
    return restored;
}

} // namespace sf
