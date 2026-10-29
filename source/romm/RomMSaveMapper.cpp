#include "romm/RomMSaveMapper.hpp"
#include "romm/RomMConfig.hpp"
#include "romm/RomMClient.hpp"
#include "cloud/RetroArchPaths.hpp"
#include "app/Config.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Hash.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>

namespace sf::romm {

RomMSaveMapper::RomMSaveMapper(const RomMConfig& config) : config_(config) {
    loadMappings();
}

bool RomMSaveMapper::loadMappings() {
    mappings_.clear();
    rommIdToIndex_.clear();
    localPathToIndex_.clear();

    const std::string path = mappingsFilePath();
    if (!FileSystem::exists(path)) {
        // Migration: older builds used "romm_library.json" for both the library cache and the
        // mappings, so whichever service saved last silently overwrote the other — sync then
        // reported "nothing to sync yet" after a restart. Adopt a legacy file that actually
        // contains mappings and move it to the dedicated mapping path.
        const std::string legacy = std::string(paths::DATA_DIR) + "/romm_library.json";
        if (legacy != path && FileSystem::exists(legacy)) {
            try {
                const auto legacyJson = Json::parse(FileSystem::readFile(legacy));
                if (legacyJson.is_object() && legacyJson.contains("mappings") &&
                    legacyJson["mappings"].is_array() && legacyJson["mappings"].size() > 0) {
                    if (FileSystem::createDirectories(FileSystem::parentPath(path)) &&
                        FileSystem::writeFile(path, legacyJson.dump(2)))
                        SF_LOG_I("RomM", "Migrated ROM mappings from legacy %s to %s",
                                 legacy.c_str(), path.c_str());
                }
            } catch (const std::exception&) {}
        }
        if (!FileSystem::exists(path)) {
            SF_LOG_I("RomM", "No mapping file at %s", path.c_str());
            return true;
        }
    }

    try {
        const auto j = Json::parse(FileSystem::readFile(path));

        // Accept both {"mappings": [...]} and a bare top-level array.
        const Json* array = nullptr;
        if (j.is_array())
            array = &j;
        else if (j.contains("mappings") && j["mappings"].is_array())
            array = &j["mappings"];

        if (!array) {
            SF_LOG_W("RomM", "Invalid mapping file format");
            return false;
        }

        for (const auto& m : *array) {
            RomMapping mapping;
            mapping.rommRomId = m.value("romm_rom_id", std::string());
            mapping.systemId = m.value("system_id", std::string());
            mapping.romPath = m.value("rom_path", std::string());
            mapping.displayName = m.value("display_name", std::string());
            mapping.coreId = m.value("core_id", std::string());
            mapping.hash = m.value("hash", std::string());

            if (!mapping.rommRomId.empty() && !mapping.romPath.empty()) {
                const size_t idx = mappings_.size();
                mappings_.push_back(std::move(mapping));
                rommIdToIndex_[mappings_.back().rommRomId] = idx;
                localPathToIndex_[mappings_.back().romPath] = idx;
            }
        }
        SF_LOG_I("RomM", "Loaded %zu ROM mappings", mappings_.size());
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_E("RomM", "Failed to load mappings: %s", ex.what());
        return false;
    }
}

bool RomMSaveMapper::saveMappings() {
    Json root = Json::object();
    Json arr = Json::array();

    for (const auto& m : mappings_) {
        Json obj = Json::object();
        obj["romm_rom_id"] = m.rommRomId;
        obj["system_id"] = m.systemId;
        obj["rom_path"] = m.romPath;
        obj["display_name"] = m.displayName;
        obj["core_id"] = m.coreId;
        obj["hash"] = m.hash;
        arr.push_back(std::move(obj));
    }
    root["mappings"] = std::move(arr);

    const std::string path = mappingsFilePath();
    FileSystem::createDirectories(FileSystem::parentPath(path));
    return FileSystem::writeFile(path, root.dump(2));
}

std::string RomMSaveMapper::mappingsFilePath() const {
    return std::string(paths::DATA_DIR) + "/" + config_.romMappingFile;
}

void RomMSaveMapper::rebuildIndexes() {
    rommIdToIndex_.clear();
    localPathToIndex_.clear();
    for (size_t i = 0; i < mappings_.size(); ++i) {
        rommIdToIndex_[mappings_[i].rommRomId] = i;
        localPathToIndex_[mappings_[i].romPath] = i;
    }
}

std::optional<RomMapping> RomMSaveMapper::findMapping(const std::string& romPath, const std::string& systemId) {
    // Check exact path match first
    auto it = localPathToIndex_.find(romPath);
    if (it != localPathToIndex_.end()) {
        return mappings_[it->second];
    }

    // Check by system + filename (for moved ROMs)
    const std::string filename = FileSystem::filenameOf(romPath);
    for (const auto& m : mappings_) {
        if (m.systemId == systemId && FileSystem::filenameOf(m.romPath) == filename) {
            return m;
        }
    }

    return std::nullopt;
}

void RomMSaveMapper::addOrUpdateMapping(const std::string& rommRomId, const std::string& systemId,
                                        const std::string& romPath, const std::string& displayName,
                                        const std::string& coreId) {
    auto it = rommIdToIndex_.find(rommRomId);
    if (it != rommIdToIndex_.end()) {
        RomMapping& m = mappings_[it->second];
        m.systemId = systemId;
        m.romPath = romPath;
        if (!displayName.empty()) m.displayName = displayName;
        if (!coreId.empty()) m.coreId = coreId;
    } else {
        RomMapping m;
        m.rommRomId = rommRomId;
        m.systemId = systemId;
        m.romPath = romPath;
        m.displayName = displayName;
        m.coreId = coreId;
        const size_t idx = mappings_.size();
        mappings_.push_back(std::move(m));
        rommIdToIndex_[mappings_[idx].rommRomId] = idx;
        localPathToIndex_[mappings_[idx].romPath] = idx;
    }
    saveMappings();
}

std::optional<RomMapping> RomMSaveMapper::getMappingByRomMId(const std::string& rommRomId) const {
    auto it = rommIdToIndex_.find(rommRomId);
    if (it != rommIdToIndex_.end()) {
        return mappings_[it->second];
    }
    return std::nullopt;
}

std::vector<LocalSaveFile> RomMSaveMapper::discoverSaves(const std::string& romPath, const std::string& systemId,
                                                         const std::string& coreId) {
    std::vector<LocalSaveFile> saves;

    auto mappingOpt = findMapping(romPath, systemId);
    if (!mappingOpt) {
        SF_LOG_W("RomM", "No mapping for ROM: %s", romPath.c_str());
        return saves;
    }
    const std::string rommRomId = mappingOpt->rommRomId;

    const auto roots = sf::cloud::discoverRetroArchSaveRoots();
    const std::string savesDir = roots.savesDir;
    const std::string statesDir = roots.statesDir;

    // Central saves
    if (FileSystem::exists(savesDir)) {
        for (const auto& entry : FileSystem::listDirectory(savesDir)) {
            if (entry.isDirectory) continue;
            if (!isSaveFile(entry.name)) continue;

            const std::string slot = slotFromFilename(entry.name);
            LocalSaveFile save;
            save.localPath = entry.path;
            save.rommRomId = rommRomId;
            save.slot = slot;
            save.type = "save";
            save.coreId = "";
            save.hash = computeHash(entry.path);
            save.size = static_cast<int64_t>(entry.size);
            save.modifiedTime = 0; // FileSystem doesn't provide modifiedTime
            saves.push_back(std::move(save));
        }
    }

    // Central states
    if (FileSystem::exists(statesDir)) {
        for (const auto& entry : FileSystem::listDirectory(statesDir)) {
            if (entry.isDirectory) continue;
            if (!isStateFile(entry.name)) continue;

            const std::string slot = slotFromFilename(entry.name);
            LocalSaveFile save;
            save.localPath = entry.path;
            save.rommRomId = rommRomId;
            save.slot = slot;
            save.type = "state";
            save.coreId = coreIdFromStateFile(entry.name).empty() ? coreId : coreIdFromStateFile(entry.name);
            save.hash = computeHash(entry.path);
            save.size = static_cast<int64_t>(entry.size);
            save.modifiedTime = 0;
            saves.push_back(std::move(save));
        }
    }

    // ROM-folder sidecar saves
    const std::string romDir = FileSystem::parentPath(romPath);
    if (FileSystem::exists(romDir)) {
        for (const auto& entry : FileSystem::listDirectory(romDir)) {
            if (entry.isDirectory) continue;
            if (!isRomSidecarSave(entry.name)) continue;

            const std::string slot = slotFromFilename(entry.name);
            LocalSaveFile save;
            save.localPath = entry.path;
            save.rommRomId = rommRomId;
            save.slot = slot;
            save.type = isStateFile(entry.name) ? "state" : "save";
            save.coreId = save.type == "state" ? coreId : "";
            save.hash = computeHash(entry.path);
            save.size = static_cast<int64_t>(entry.size);
            save.modifiedTime = 0;
            saves.push_back(std::move(save));
        }
    }

    return saves;
}

std::vector<LocalSaveFile> RomMSaveMapper::discoverAllSaves() {
    std::vector<LocalSaveFile> allSaves;
    for (const auto& mapping : mappings_) {
        const auto saves = discoverSaves(mapping.romPath, mapping.systemId, mapping.coreId);
        allSaves.insert(allSaves.end(), saves.begin(), saves.end());
    }
    return allSaves;
}

std::optional<std::string> RomMSaveMapper::resolveLocalPath(const RomMSave& save) const {
    // Try to find by rommRomId + slot + type
    for (const auto& mapping : mappings_) {
        if (mapping.rommRomId != save.romId) continue;

        // Check central saves/states
        const auto roots = sf::cloud::discoverRetroArchSaveRoots();
        std::string baseDir = save.type == "state" ? roots.statesDir : roots.savesDir;
        std::string expectedName = mapping.displayName + "." + save.slot;
        if (save.type == "state") expectedName += ".state";

        std::string candidate = FileSystem::join(baseDir, expectedName);
        if (FileSystem::exists(candidate)) return candidate;

        // Check ROM-folder sidecar
        const std::string romDir = FileSystem::parentPath(mapping.romPath);
        candidate = FileSystem::join(romDir, expectedName);
        if (FileSystem::exists(candidate)) return candidate;
    }
    return std::nullopt;
}

std::string RomMSaveMapper::computeHash(const std::string& path) {
    return sf::Hash::md5File(path);
}

std::string RomMSaveMapper::getSaveDir(const std::string& systemId) const {
    const auto roots = sf::cloud::discoverRetroArchSaveRoots();
    return roots.savesDir;
}

std::string RomMSaveMapper::getStateDir(const std::string& systemId) const {
    const auto roots = sf::cloud::discoverRetroArchSaveRoots();
    return roots.statesDir;
}

std::string RomMSaveMapper::slotFromFilename(const std::string& filename) {
    // Extract slot from filename like "gamename.srm" -> "0", "gamename.auto.srm" -> "auto"
    size_t lastDot = filename.rfind('.');
    if (lastDot == std::string::npos) return "0";

    std::string base = filename.substr(0, lastDot);
    size_t prevDot = base.rfind('.');
    if (prevDot == std::string::npos) return "0";

    return base.substr(prevDot + 1);
}

bool RomMSaveMapper::isSaveFile(const std::string& filename) {
    const std::string lower = toLower(filename);
    static const std::vector<std::string> saveExts = {".srm", ".sav", ".eep", ".rtc", ".fla", ".nvram"};
    for (const auto& ext : saveExts) {
        if (endsWith(lower, ext)) return true;
    }
    return false;
}

bool RomMSaveMapper::isStateFile(const std::string& filename) {
    const std::string lower = toLower(filename);
    return endsWith(lower, ".state") || lower.find(".state.") != std::string::npos;
}

bool RomMSaveMapper::isRomSidecarSave(const std::string& filename) {
    return isSaveFile(filename) || isStateFile(filename);
}

std::string RomMSaveMapper::coreIdFromStateFile(const std::string& filename) {
    // State files may be named like "gamename.coreid.state" or "gamename.state"
    const std::string lower = toLower(filename);
    size_t statePos = lower.rfind(".state");
    if (statePos == std::string::npos) return "";

    std::string beforeState = lower.substr(0, statePos);
    size_t dotPos = beforeState.rfind('.');
    if (dotPos == std::string::npos) return "";

    return beforeState.substr(dotPos + 1);
}

std::string RomMSaveMapper::toLower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool RomMSaveMapper::endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace sf::romm
