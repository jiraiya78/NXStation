#pragma once

#include <string>
#include <optional>
#include <ctime>

namespace sf::romm {

/** Conflict resolution policy for save synchronization. */
enum class ConflictPolicy {
    KeepBoth,   // Archive local copy, keep remote
    KeepLocal,  // Keep local, don't upload
    KeepRemote, // Download remote, overwrite local
    Ask         // Prompt user (not yet implemented)
};

/** RomM sync configuration stored in user settings. */
struct RomMConfig {
    bool enabled = false;
    std::string serverUrl;          // e.g. "https://romm.example.com"
    std::string deviceId;           // Assigned after registration
    std::string deviceName;         // Friendly name for this device
    bool syncSaves = true;          // Sync in-game save files (.srm, .sav, etc.)
    bool syncStates = true;         // Sync emulator save states
    bool syncBeforeLaunch = true;   // Download before launching a game
    bool syncAfterReturn = true;    // Upload on next NXStation startup
    ConflictPolicy conflictPolicy = ConflictPolicy::KeepBoth;
    bool stateCoreMatching = true;  // Only restore states to compatible cores
    std::string romMappingFile = "romm_mappings.json"; // ROM ID mapping file (must not collide with the library cache)

    // Runtime-only (not persisted to config file)
    std::string accessToken;        // Bearer token for API calls
    std::string clientToken;        // Pairing client token
    time_t tokenExpiresAt = 0; // Token expiration
};

/** RomM sync state persisted separately from config. */
struct RomMSyncState {
    std::string deviceId;
    std::string lastSyncSessionId;
    time_t lastSuccessfulSync = 0;
    std::string lastSyncResult;     // Human-readable summary
};

/** Convert ConflictPolicy to string for storage. */
std::string conflictPolicyToString(ConflictPolicy policy);

/** Convert string to ConflictPolicy. */
ConflictPolicy conflictPolicyFromString(const std::string& str);

/** Default configuration file path. */
std::string defaultConfigPath();

/** Default sync state file path. */
std::string defaultSyncStatePath();

} // namespace sf::romm
