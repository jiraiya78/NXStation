#include "romm/RomMConfig.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Paths.hpp"
#include "util/Logger.hpp"

#include <fstream>
#include <ctime>

namespace sf::romm {

std::string conflictPolicyToString(ConflictPolicy policy) {
    switch (policy) {
        case ConflictPolicy::KeepBoth: return "keep_both";
        case ConflictPolicy::KeepLocal: return "keep_local";
        case ConflictPolicy::KeepRemote: return "keep_remote";
        case ConflictPolicy::Ask: return "ask";
    }
    return "keep_both";
}

ConflictPolicy conflictPolicyFromString(const std::string& str) {
    if (str == "keep_local") return ConflictPolicy::KeepLocal;
    if (str == "keep_remote") return ConflictPolicy::KeepRemote;
    if (str == "ask") return ConflictPolicy::Ask;
    return ConflictPolicy::KeepBoth;
}

std::string defaultConfigPath() {
    return std::string(paths::SETTINGS_DIR) + "/romm_config.json";
}

std::string defaultSyncStatePath() {
    return std::string(paths::DATA_DIR) + "/romm_sync_state.json";
}

} // namespace sf::romm
