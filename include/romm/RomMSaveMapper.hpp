#pragma once

#include "romm/RomMConfig.hpp"
#include "romm/RomMClient.hpp"

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace sf::romm {

/** Mapping between RomM ROM ID and local game identity. */
struct RomMapping {
    std::string rommRomId;       // RomM's ROM identifier
    std::string systemId;        // NXStation system ID (e.g. "nes", "snes")
    std::string romPath;         // Local ROM file path
    std::string displayName;     // Game display name
    std::string coreId;          // Core identifier for save state compatibility
    std::string hash;            // ROM file hash for verification
};

/** Save file metadata for sync. */
struct LocalSaveFile {
    std::string localPath;       // Absolute path to save file
    std::string rommRomId;       // Mapped RomM ROM ID
    std::string slot;            // Save slot (e.g. "0", "auto")
    std::string type;            // "save" or "state"
    std::string coreId;          // Core ID (for states)
    std::string hash;            // File hash
    int64_t size = 0;
    time_t modifiedTime = 0;
};

/** Save mapper - handles ROM identity and save/state path discovery. */
class RomMSaveMapper {
public:
    RomMSaveMapper(const RomMConfig& config);

    /** Load ROM mappings from file. */
    bool loadMappings();

    /** Save ROM mappings to file. */
    bool saveMappings();

    /** Find or create mapping for a local ROM. */
    std::optional<RomMapping> findMapping(const std::string& romPath, const std::string& systemId);

    /** Register or update the mapping for a RomM ROM downloaded to a local path. */
    void addOrUpdateMapping(const std::string& rommRomId, const std::string& systemId,
                            const std::string& romPath, const std::string& displayName,
                            const std::string& coreId);

    /** Get mapping by RomM ROM ID. */
    std::optional<RomMapping> getMappingByRomMId(const std::string& rommRomId) const;

    /** Get all mappings. */
    const std::vector<RomMapping>& allMappings() const { return mappings_; }

    /** Discover all local save files for a ROM. */
    std::vector<LocalSaveFile> discoverSaves(const std::string& romPath, const std::string& systemId,
                                             const std::string& coreId);

    /** Discover save files for all mapped ROMs. */
    std::vector<LocalSaveFile> discoverAllSaves();

    /** Resolve local save path from RomM save metadata. */
    std::optional<std::string> resolveLocalPath(const RomMSave& save) const;

    /** Compute hash of a file. */
    static std::string computeHash(const std::string& path);

    /** Get save directory for a system. */
    std::string getSaveDir(const std::string& systemId) const;

    /** Get state directory for a system. */
    std::string getStateDir(const std::string& systemId) const;

    /** Determine save slot from filename. */
    static std::string slotFromFilename(const std::string& filename);

    /** Determine if file is a save file. */
    static bool isSaveFile(const std::string& filename);

    /** Determine if file is a save state. */
    static bool isStateFile(const std::string& filename);

    /** Determine if file is a ROM sidecar save. */
    static bool isRomSidecarSave(const std::string& filename);

    /** Extract core ID from state filename (if present). */
    static std::string coreIdFromStateFile(const std::string& filename);

private:
    const RomMConfig& config_;
    std::vector<RomMapping> mappings_;
    std::unordered_map<std::string, size_t> rommIdToIndex_;
    std::unordered_map<std::string, size_t> localPathToIndex_;

    std::string mappingsFilePath() const;
    void rebuildIndexes();

    static std::string toLower(std::string value);
    static bool endsWith(const std::string& value, const std::string& suffix);
};

} // namespace sf::romm
