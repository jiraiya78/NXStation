#pragma once

#include <switch.h>

#include <string>
#include <vector>

namespace sf {

struct ForwarderConfig {
    std::string nro_path;
    std::string args;
    std::string name;
    std::string author;
    NacpStruct nacp{};
    std::vector<u8> icon;
    std::vector<u8> logo;
    std::vector<u8> gif;
    std::vector<u8> program_nca;
};

/**
 * Title ID a forwarder for this NRO gets. Must be fed the same nro_path/args a
 * ForwarderConfig carries *before* install, since install rewrites args.
 */
u64 forwarderTitleId(const std::string& nro_path, const std::string& args);

/** Install a main-menu forwarder (on-device NCA generation). */
Result installForwarderNca(ForwarderConfig& config, NcmStorageId storage_id);

} // namespace sf
