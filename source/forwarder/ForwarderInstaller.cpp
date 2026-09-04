#include "forwarder/ForwarderInstaller.hpp"
#include "forwarder/ForwarderNca.hpp"
#include "forwarder/NroMeta.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <cstring>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf::ForwarderInstaller {

namespace {

constexpr const char* KEYS_CANDIDATES[] = {
    "sdmc:/switch/prod.keys",
    "sdmc:/switch/.prod.keys",
    "sdmc:/switch/prod.keys.template",
};

bool report(std::function<void(const std::string&)>& cb, const std::string& line)
{
    SF_LOG_I("Forwarder", "%s", line.c_str());
    if (cb)
        cb(line);
    return true;
}

#ifdef __SWITCH__

// The path string the forwarder is built from; also what hbloader receives as argv.
std::string forwarderNroPath()
{
    return nroArgvPath(paths::APP_NRO);
}

bool hasApplicationRecord(u64 tid)
{
    s32 count = 0;
    if (R_SUCCEEDED(nsCountApplicationContentMeta(tid, &count)) && count > 0)
        return true;

    // Some firmwares reject the count query for non-eShop title IDs; walk the
    // Home Menu record list instead.
    std::vector<NsApplicationRecord> records(64);
    s32 offset = 0;
    while (true) {
        s32 read = 0;
        if (R_FAILED(nsListApplicationRecord(records.data(), static_cast<s32>(records.size()),
                                             offset, &read))
            || read <= 0)
            break;

        for (s32 i = 0; i < read; ++i) {
            if (records[i].application_id == tid)
                return true;
        }
        offset += read;
    }

    return false;
}

#endif

} // namespace

bool isForwarderInstalled()
{
#ifdef __SWITCH__
    if (R_FAILED(nsInitialize()))
        return false;

    const u64 tid = forwarderTitleId(forwarderNroPath(), "");
    const bool found = hasApplicationRecord(tid);
    nsExit();

    SF_LOG_I("Forwarder", "detect tid=%016llX installed=%d", static_cast<unsigned long long>(tid),
             found ? 1 : 0);
    return found;
#else
    return false;
#endif
}

bool keysAvailable()
{
    for (const char* path : KEYS_CANDIDATES) {
        if (FileSystem::exists(path))
            return true;
    }
    return false;
}

bool installNxStation(std::string& errorOut, std::function<void(const std::string&)> onProgress)
{
    if (!keysAvailable()) {
        errorOut =
            "prod.keys not found.\n\n"
            "Copy your keys to sdmc:/switch/prod.keys, then run Install Forwarder again.";
        report(onProgress, "prod.keys not found.");
        return false;
    }

#ifdef __SWITCH__
    if (!FileSystem::exists(paths::APP_NRO)) {
        errorOut = "NXStation.nro not found at:\n" + std::string(paths::APP_NRO);
        return false;
    }

    report(onProgress, "Reading NXStation metadata…");

    // Leave args empty: installForwarderNca derives them from nro_path, and the
    // title ID hash depends on that exact pairing.
    ForwarderConfig config{};
    config.nro_path = forwarderNroPath();

    if (R_FAILED(readNroNacp(config.nro_path, config.nacp))) {
        std::memset(&config.nacp, 0, sizeof(config.nacp));
        std::strncpy(config.nacp.lang[0].name, "NXStation", sizeof(config.nacp.lang[0].name) - 1);
        std::strncpy(config.nacp.lang[0].author, "NXStation",
                     sizeof(config.nacp.lang[0].author) - 1);
    }

    config.name = config.nacp.lang[0].name;
    config.author = config.nacp.lang[0].author;
    config.icon = readNroIcon(config.nro_path);
    if (config.icon.empty()) {
        errorOut = "Could not read icon from NXStation.nro";
        return false;
    }

    report(onProgress, "Installing main-menu forwarder…");

    const NcmStorageId storage = NcmStorageId_SdCard;
    const Result rc = installForwarderNca(config, storage);
    if (R_FAILED(rc)) {
        errorOut = "Forwarder install failed (0x" + std::to_string(static_cast<unsigned>(rc)) + ")";
        return false;
    }

    errorOut =
        "NXStation forwarder installed.\n\n"
        "An NXStation icon should now appear on the Home Menu. Launch NXStation from "
        "that icon so games return to NXStation when you close them.";
    report(onProgress, "Forwarder installed.");
    return true;
#else
    errorOut = "Forwarder install is only available on Switch.";
    return false;
#endif
}

} // namespace sf::ForwarderInstaller
