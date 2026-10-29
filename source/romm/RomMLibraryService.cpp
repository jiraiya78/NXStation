#include "romm/RomMLibraryService.hpp"
#include "romm/RomMClient.hpp"
#include "romm/RomMConfig.hpp"
#include "app/Config.hpp"
#include "cloud/ZipArchive.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>

namespace sf::romm {

namespace {

int64_t cacheNumberField(const Json& obj, const std::string& key)
{
    if (!obj.contains(key) || !obj[key].is_number())
        return 0;
    return static_cast<int64_t>(obj[key].get<double>());
}

} // namespace

RomMLibraryService::RomMLibraryService(RomMClient& client, const RomMConfig& config)
    : client_(client), config_(config) {}

bool RomMLibraryService::fetchLibrary(std::vector<LibraryEntry>& out,
                                      std::function<void(int, int)> progress) {
    std::vector<RomMLibraryEntry> roms;
    if (!client_.fetchLibrary(roms, [&progress](int fetched, int total) {
            if (progress) progress(fetched, total);
        })) {
        return false;
    }

    out.reserve(roms.size());
    for (size_t i = 0; i < roms.size(); ++i) {
        if (progress) progress(static_cast<int>(i + 1), static_cast<int>(roms.size()));

        LibraryEntry entry;
        entry.rom = std::move(roms[i]);
        out.push_back(std::move(entry));
    }

    matchToSystems(out);
    refreshDownloadedFlags(out);

    // Persist as the main-page snapshot (works offline on next launch).
    entries_ = out;
    saveCache();

    if (progress) progress(static_cast<int>(out.size()), static_cast<int>(out.size()));
    return true;
}

bool RomMLibraryService::loadCache() {
    entries_.clear();

    const std::string path = cacheFilePath();
    if (!FileSystem::exists(path))
        return false;

    try {
        const auto j = Json::parse(FileSystem::readFile(path));
        if (!j.contains("entries") || !j["entries"].is_array())
            return false;

        for (const auto& item : j["entries"]) {
            LibraryEntry entry;
            entry.rom.id = item.value("id", std::string());
            entry.rom.name = item.value("name", std::string());
            entry.rom.platform = item.value("platform", std::string());
            entry.rom.region = item.value("region", std::string());
            entry.rom.version = item.value("version", std::string());
            entry.rom.totalSize = static_cast<int64_t>(item.value("total_size", 0.0));
            entry.rom.folderName = item.value("folder_name", std::string());
            entry.rom.fsName = item.value("fs_name", std::string());
            entry.rom.summary = item.value("summary", std::string());
            entry.rom.multiFile = item.contains("multi_file") && item["multi_file"].is_boolean()
                                       ? item["multi_file"].get<bool>() : false;
            if (item.contains("files") && item["files"].is_array()) {
                for (const auto& f : item["files"]) {
                    RomFileEntry file;
                    if (f.is_string()) {
                        file.fileName = f.get<std::string>();
                    } else if (f.is_object()) {
                        file.id = static_cast<long long>(cacheNumberField(f, "id"));
                        file.fileName = f.value("file_name", std::string());
                        file.sizeBytes = static_cast<int64_t>(f.value("file_size_bytes", 0.0));
                        file.topLevel = f.contains("is_top_level") && f["is_top_level"].is_boolean()
                                            ? f["is_top_level"].get<bool>() : true;
                    }
                    if (!file.fileName.empty()) entry.rom.files.push_back(std::move(file));
                }
            }
            // Older cache files stored no file list; rebuild it from fs_name for single-file ROMs.
            if (entry.rom.files.empty() && !entry.rom.fsName.empty()) {
                RomFileEntry file;
                file.fileName = entry.rom.fsName;
                file.sizeBytes = entry.rom.totalSize;
                entry.rom.files.push_back(std::move(file));
            }
            entry.matchedSystemId = item.value("matched_system_id", std::string());
            entry.localPath = item.value("local_path", std::string());
            entry.downloadedLocally = item.value("downloaded", false) &&
                                      !entry.localPath.empty() && FileSystem::exists(entry.localPath);

            if (!entry.rom.id.empty()) entries_.push_back(std::move(entry));
        }
        SF_LOG_I("RomM", "Loaded %zu cached library entries", entries_.size());
        return !entries_.empty();
    } catch (const std::exception& ex) {
        SF_LOG_E("RomM", "Failed to load library cache: %s", ex.what());
        return false;
    }
}

bool RomMLibraryService::saveCache() const {
    Json root = Json::object();
    root["fetched_at"] = static_cast<double>(std::time(nullptr));

    Json arr = Json::array();
    for (const auto& entry : entries_) {
        Json obj = Json::object();
        obj["id"] = entry.rom.id;
        obj["name"] = entry.rom.name;
        obj["platform"] = entry.rom.platform;
        obj["region"] = entry.rom.region;
        obj["version"] = entry.rom.version;
        obj["total_size"] = static_cast<double>(entry.rom.totalSize);
        obj["folder_name"] = entry.rom.folderName;
        obj["fs_name"] = entry.rom.fsName;
        obj["multi_file"] = entry.rom.multiFile;
        obj["summary"] = entry.rom.summary;
        Json files = Json::array();
        for (const auto& f : entry.rom.files) {
            Json file = Json::object();
            file["id"] = static_cast<double>(f.id);
            file["file_name"] = f.fileName;
            file["file_size_bytes"] = static_cast<double>(f.sizeBytes);
            file["is_top_level"] = f.topLevel;
            files.push_back(std::move(file));
        }
        obj["files"] = std::move(files);
        obj["matched_system_id"] = entry.matchedSystemId;
        obj["local_path"] = entry.localPath;
        obj["downloaded"] = entry.downloadedLocally;
        arr.push_back(std::move(obj));
    }
    root["entries"] = std::move(arr);

    const std::string path = cacheFilePath();
    FileSystem::createDirectories(FileSystem::parentPath(path));
    return FileSystem::writeFile(path, root.dump(2));
}

const LibraryEntry* RomMLibraryService::entryByRomId(const std::string& romId) const {
    for (const auto& entry : entries_) {
        if (entry.rom.id == romId)
            return &entry;
    }
    return nullptr;
}

void RomMLibraryService::markNotDownloaded(const std::string& romId) {
    // entries_ is const-visible here; mutate through a non-const view of the same vector.
    auto& mutableEntries = const_cast<std::vector<LibraryEntry>&>(entries_);
    for (auto& entry : mutableEntries) {
        if (entry.rom.id == romId) {
            entry.downloadedLocally = false;
            entry.localPath.clear();
            break;
        }
    }
    saveCache();
}

std::string RomMLibraryService::romFolderName(const RomMLibraryEntry& rom) {
    // RomM's fs_path is relative to the library root, e.g. "psx/Chrono Cross (USA)".
    std::string folder = rom.folderName;
    const size_t pos = folder.find_last_of('/');
    if (pos != std::string::npos)
        folder = folder.substr(pos + 1);
    if (folder.empty())
        folder = rom.name;
    for (char& c : folder) {
        if (c == '/')
            c = '_';
    }
    return folder;
}

std::vector<std::string> RomMLibraryService::candidateDirsFor(const LibraryEntry& entry) const {
    std::vector<std::string> dirs;
    const std::string systemDir = downloadDirFor(entry.matchedSystemId);
    dirs.push_back(systemDir);

    // A multi-file ROM may have been extracted into its own folder.
    const std::string folder = romFolderName(entry.rom);
    if (!folder.empty()) {
        dirs.push_back(FileSystem::join(systemDir, folder));
        // The legacy RomM download root, for copies made by older builds.
        const std::string root = std::string(paths::ROMM_DOWNLOAD_DIR) + "/" +
                                 (entry.matchedSystemId.empty() ? "unmatched" : entry.matchedSystemId);
        if (root != systemDir) {
            dirs.push_back(root);
            dirs.push_back(FileSystem::join(root, folder));
        }
    }
    return dirs;
}

std::string RomMLibraryService::downloadDirFor(const std::string& systemId) const {
    // Downloads must land where the scanner looks: the configured ROM folder for the system
    // (sdmc:/roms/{systemId}). Unmatched platforms fall back to the RomM download root.
    const SystemConfig* sys = sf::Config::instance().findSystem(systemId);
    if (sys && !sys->path.empty())
        return sys->path;
    return std::string(paths::ROMM_DOWNLOAD_DIR) + "/" + (systemId.empty() ? "unmatched" : systemId);
}

std::string RomMLibraryService::cacheFilePath() const {
    return std::string(paths::DATA_DIR) + "/romm_library.json";
}

void RomMLibraryService::refreshDownloadedFlags(std::vector<LibraryEntry>& entries) {
    for (auto& entry : entries) {
        entry.localPath.clear();
        entry.downloadedLocally = false;

        if (!entry.rom.files.empty()) {
            const std::string folder = romFolderName(entry.rom);
            for (const std::string& dir : candidateDirsFor(entry)) {
                bool foundInFolder = false;
                for (const auto& file : entry.rom.files) {
                    const std::string candidate = FileSystem::join(dir, file.fileName);
                    if (FileSystem::exists(candidate)) {
                        entry.localPath = candidate;
                        entry.downloadedLocally = true;
                        foundInFolder = true;
                        break;
                    }
                    // Folder-keeping scanners store the ROM inside its RomM folder.
                    const std::string nested = folder.empty()
                        ? ""
                        : FileSystem::join(FileSystem::join(dir, folder), file.fileName);
                    if (!nested.empty() && FileSystem::exists(nested)) {
                        entry.localPath = nested;
                        entry.downloadedLocally = true;
                        foundInFolder = true;
                        break;
                    }
                }
                if (foundInFolder)
                    break;
            }
        }

        // Also accept copies already present in a scanned system folder.
        if (!entry.downloadedLocally)
            entry.downloadedLocally = isRomDownloaded(entry.rom, entry.localPath);
    }
}

void RomMLibraryService::matchToSystems(std::vector<LibraryEntry>& entries) {
    for (auto& entry : entries) {
        entry.matchedSystemId = matchPlatformToSystem(entry.rom.platform).value_or("");
    }
}

namespace {

/** Percent-encode a single URL path segment (RomM file names commonly contain spaces). */
std::string encodePathSegment(const std::string& value) {
    static const char* safe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (strchr(safe, c)) {
            out.push_back(static_cast<char>(c));
        } else if (c == '/') {
            out.push_back('/');
        } else {
            out += "%";
            out += "0123456789ABCDEF"[(c >> 4) & 0xF];
            out += "0123456789ABCDEF"[c & 0xF];
        }
    }
    return out;
}

/** File name suitable as the ZIP output name for a multi-part ROM. */
std::string zipNameFor(const RomMLibraryEntry& rom) {
    std::string name = RomMLibraryService::romFolderName(rom);
    if (name.empty())
        name = rom.name;
    for (char& c : name) {
        if (c == '/')
            c = '_';
    }
    return name + ".zip";
}

} // namespace

bool RomMLibraryService::downloadRom(const LibraryEntry& entry, const std::string& destDir,
                                     const DownloadProgressFn& progress,
                                     std::atomic<bool>* abort) {
    return downloadRomFiles(entry, {}, destDir, progress, abort);
}

bool RomMLibraryService::resolveFiles(LibraryEntry& entry) {
    if (!entry.rom.files.empty())
        return true;

    // Some RomM builds do not include the `files` array in the paginated /api/roms response.
    // Single-rom SimpleRomSchema does, so fetch it — that is also where the file ids used by
    // ?file_ids= and the content endpoint come from.
    RomMLibraryEntry remote;
    if (!client_.fetchSimpleRom(entry.rom.id, remote) || remote.files.empty())
        return false;

    entry.rom.files = remote.files;
    if (entry.rom.totalSize == 0)
        entry.rom.totalSize = remote.totalSize;
    if (entry.rom.folderName.empty())
        entry.rom.folderName = remote.folderName;
    if (entry.rom.fsName.empty())
        entry.rom.fsName = remote.fsName;
    entry.rom.multiFile = remote.multiFile || entry.rom.files.size() > 1;

    // Keep the cache in step, so the row's disc count and the picker see the files too.
    auto& mutableEntries = const_cast<std::vector<LibraryEntry>&>(entries_);
    for (auto& cached : mutableEntries) {
        if (cached.rom.id == entry.rom.id) {
            cached.rom.files = entry.rom.files;
            cached.rom.fsName = entry.rom.fsName;
            cached.rom.multiFile = entry.rom.multiFile;
            break;
        }
    }
    saveCache();

    SF_LOG_I("RomM", "Resolved %zu file(s) for %s via /roms/%s/simple",
             entry.rom.files.size(), entry.rom.name.c_str(), entry.rom.id.c_str());
    return true;
}

bool RomMLibraryService::downloadRomFiles(const LibraryEntry& entryIn,
                                          const std::vector<long long>& fileIds,
                                          const std::string& destDir,
                                          const DownloadProgressFn& progress,
                                          std::atomic<bool>* abort) {
    LibraryEntry entry = entryIn;
    if (entry.rom.files.empty())
        resolveFiles(entry);

    std::vector<RomFileEntry> selected;
    for (const auto& f : entry.rom.files) {
        if (fileIds.empty() || std::find(fileIds.begin(), fileIds.end(), f.id) != fileIds.end())
            selected.push_back(f);
    }
    if (selected.empty()) {
        SF_LOG_E("RomM", "ROM has no files: %s", entry.rom.id.c_str());
        return false;
    }

    FileSystem::createDirectories(destDir);

    std::string launchTarget;

    if (selected.size() > 1) {
        // RomM serves multi-part ROMs (including .m3u sets) as a ZIP, so download the ZIP
        // and extract it into the ROM's folder inside the scanned system folder.
        const std::string zipName = zipNameFor(entry.rom);
        const std::string zipPath = FileSystem::join(destDir, zipName);

        std::string url = client_.buildUrlPublic("/roms/" + entry.rom.id + "/content/" +
                                                 encodePathSegment(zipName));
        if (!fileIds.empty()) {
            std::string ids;
            for (long long id : fileIds) {
                if (!ids.empty())
                    ids += ",";
                ids += std::to_string(id);
            }
            url += "?file_ids=" + ids;
        }

        DownloadProgress base;
        base.fileIndex = 1;
        base.totalFiles = 1;
        base.fileName = zipName;
        base.totalBytes = entry.rom.totalSize;

        if (!downloadFileAtomic(url, zipPath, base, progress, abort))
            return false;

        const std::string outDir = FileSystem::join(destDir, romFolderName(entry.rom));
        sf::cloud::ZipArchive zip;
        int extracted = 0;
        const int totalEntries = static_cast<int>(selected.size());

        const bool ok = zip.extractMerge(
            zipPath,
            [&](const std::string& zipEntry) -> std::optional<std::string> {
                if (abort && abort->load())
                    return std::nullopt;
                // Entries keep the set folder; flatten so the .m3u and discs stay together.
                return FileSystem::join(outDir, FileSystem::filenameOf(zipEntry));
            },
            [&](const std::string& zipEntry, const std::string& target, bool entryOk, bool skipped) {
                if (abort && abort->load())
                    return;
                if (!entryOk) {
                    SF_LOG_W("RomM", "ZIP entry failed: %s", zipEntry.c_str());
                    return;
                }
                if (skipped)
                    return;
                ++extracted;
                if (progress) {
                    DownloadProgress p;
                    p.fileIndex = extracted;
                    p.totalFiles = totalEntries;
                    p.fileName = FileSystem::filenameOf(zipEntry);
                    progress(p);
                }
            },
            abort);

        FileSystem::removeFile(zipPath);
        if (!ok)
            return false;

        // Launch target: the set's .m3u when present, otherwise the first top-level file.
        for (const auto& f : selected) {
            const std::string ext = FileSystem::toLower(FileSystem::extensionOf(f.fileName));
            if (ext == ".m3u") {
                launchTarget = FileSystem::join(outDir, f.fileName);
                break;
            }
        }
        if (launchTarget.empty()) {
            for (const auto& f : selected) {
                if (f.topLevel) {
                    launchTarget = FileSystem::join(outDir, f.fileName);
                    break;
                }
            }
        }
        if (launchTarget.empty())
            launchTarget = FileSystem::join(outDir, selected.front().fileName);
    } else {
        const std::string fileName = selected.front().fileName;
        launchTarget = FileSystem::join(destDir, fileName);

        DownloadProgress base;
        base.fileIndex = 1;
        base.totalFiles = 1;
        base.fileName = fileName;
        base.totalBytes = selected.front().sizeBytes;

        const std::string url = client_.buildUrlPublic("/roms/" + entry.rom.id + "/content/" +
                                                       encodePathSegment(fileName));
        if (!downloadFileAtomic(url, launchTarget, base, progress, abort))
            return false;
    }

    // Mark the cache entry as downloaded so the main page reflects it.
    auto& mutableEntries = const_cast<std::vector<LibraryEntry>&>(entries_);
    for (auto& cached : mutableEntries) {
        if (cached.rom.id == entry.rom.id) {
            cached.downloadedLocally = true;
            cached.localPath = launchTarget;
            cached.matchedSystemId = entry.matchedSystemId;
            break;
        }
    }
    saveCache();

    // Trigger library refresh
    triggerLibraryRefresh();
    return true;
}

bool RomMLibraryService::downloadRomFile(const std::string& romId, const std::string& fileName,
                                         const std::string& localPath,
                                         const DownloadProgressFn& progress) {
    std::string url = client_.buildUrlPublic("/roms/" + romId + "/content/" + fileName);

    DownloadProgress base;
    base.fileIndex = 1;
    base.totalFiles = 1;
    base.fileName = fileName;

    return downloadFileAtomic(url, localPath, base, progress, nullptr);
}

void RomMLibraryService::triggerLibraryRefresh() {
    // This would signal the main app to refresh its library
    // For now, we just log it. The actual refresh would be handled by the UI layer.
    SF_LOG_I("RomM", "Library refresh triggered after ROM download");
}

std::string RomMLibraryService::mappingFilePath() const {
    return std::string(paths::DATA_DIR) + "/" + config_.romMappingFile;
}

std::optional<std::string> RomMLibraryService::matchPlatformToSystem(const std::string& platform) const {
    // Map RomM platform names to NXStation system IDs
    static const std::unordered_map<std::string, std::string> platformMap = {
        {"nes", "nes"},
        {"fds", "fds"},
        {"snes", "snes"},
        {"sfc", "snes"},
        {"gameboy", "gb"},
        {"gb", "gb"},
        {"gbc", "gbc"},
        {"gameboycolor", "gbc"},
        {"gba", "gba"},
        {"gameboyadvance", "gba"},
        {"n64", "n64"},
        {"nintendo64", "n64"},
        {"nds", "nds"},
        {"nintendods", "nds"},
        {"3ds", "3ds"},
        {"nintendo3ds", "3ds"},
        {"wii", "wii"},
        {"wiiu", "wiiu"},
        {"gamecube", "gc"},
        {"gc", "gc"},
        {"genesis", "genesis"},
        {"megadrive", "genesis"},
        {"md", "genesis"},
        {"segacd", "segacd"},
        {"32x", "32x"},
        {"mastersystem", "mastersystem"},
        {"sms", "mastersystem"},
        {"gamegear", "gamegear"},
        {"gg", "gamegear"},
        {"saturn", "saturn"},
        {"dreamcast", "dreamcast"},
        {"psx", "psx"},
        {"playstation", "psx"},
        {"psp", "psp"},
        {"ps2", "ps2"},
        {"playstation2", "ps2"},
        {"ps3", "ps3"},
        {"playstation3", "ps3"},
        {"atari2600", "a2600"},
        {"a2600", "a2600"},
        {"atari7800", "a7800"},
        {"lynx", "lynx"},
        {"jaguar", "jaguar"},
        {"pcengine", "pce"},
        {"pce", "pce"},
        {"tg16", "pce"},
        {"pcfx", "pcfx"},
        {"wonderswan", "ws"},
        {"ws", "ws"},
        {"wonderswancolor", "wsc"},
        {"wsc", "wsc"},
        {"neo-geo", "neogeo"},
        {"neogeo", "neogeo"},
        {"neogeocd", "neogeocd"},
        {"ngcd", "neogeocd"},
        {"arcade", "arcade"},
        {"mame", "arcade"},
        {"fbneo", "arcade"},
        {"cps1", "arcade"},
        {"cps2", "arcade"},
        {"cps3", "arcade"},
        {"naomi", "arcade"},
        {"atomiswave", "arcade"},
        {"sega-saturn", "saturn"},
        {"dreamcast", "dreamcast"},
    };

    std::string lower = platform;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = platformMap.find(lower);
    if (it != platformMap.end()) return it->second;

    // Try to find in NXStation's configured systems
    const auto& systems = sf::Config::instance().systems();
    for (const auto& sys : systems) {
        if (sys.id == lower) return sys.id;
    }

    return std::nullopt;
}

bool RomMLibraryService::isRomDownloaded(const RomMLibraryEntry& rom, std::string& outPath) const {
    const auto& systems = sf::Config::instance().systems();
    for (const auto& sys : systems) {
        if (sys.path.empty()) continue;

        for (const auto& file : rom.files) {
            std::string candidate = FileSystem::join(sys.path, file.fileName);
            if (FileSystem::exists(candidate)) {
                outPath = candidate;
                return true;
            }
        }
    }
    return false;
}

bool RomMLibraryService::downloadFileAtomic(const std::string& url, const std::string& destPath,
                                            const DownloadProgress& base,
                                            const DownloadProgressFn& progress,
                                            std::atomic<bool>* abort) {
    if (abort && abort->load())
        return false;

    // Download to .part file first for atomic write. A leftover .part from a dropped transfer is
    // resumed with a Range header instead of being overwritten, so a long download can survive a
    // console sleep / app restart and continue from where it stopped.
    std::string partPath = destPath + ".part";
    const int64_t resumedBytes = static_cast<int64_t>(FileSystem::fileSize(partPath));

    int lastPercent = -1;
    int64_t lastReportedBytes = 0;
    int64_t observedContentTotal = 0;  // whole-file size seen on the wire when Content-Length exists
    sf::cloud::ProgressFn onBytes = [&](int64_t downloaded, int64_t total) -> bool {
        if (abort && abort->load())
            return true;
        if (!progress)
            return false;

        if (total > 0)
            observedContentTotal = total;

        // RomM streams multi-part ROMs as a ZIP, which often has no Content-Length, so `total`
        // can be 0. Percent then never changes and the UI would sit on "Starting download" for the
        // whole transfer — fall back to reporting bytes, and use the cached size as the estimate.
        const int64_t knownTotal = total > 0 ? total : base.totalBytes;
        const int percent = knownTotal > 0 ? static_cast<int>(downloaded * 100 / knownTotal) : -1;
        const bool percentChanged = percent != lastPercent;
        const bool bytesAdvanced = downloaded == 0 || downloaded - lastReportedBytes >= (1 << 20);
        if (!percentChanged && !bytesAdvanced)
            return false;
        lastPercent = percent;
        lastReportedBytes = downloaded;

        DownloadProgress p = base;
        p.downloadedBytes = downloaded;
        p.totalBytes = knownTotal;
        p.resumed = resumedBytes > 0;
        return progress(p);
    };

    if (resumedBytes > 0)
        SF_LOG_I("RomM", "Resuming %s from %lld bytes", partPath.c_str(),
                 static_cast<long long>(resumedBytes));

    SF_LOG_I("RomM", "GET %s -> %s", url.c_str(), destPath.c_str());

    if (!client_.downloadToUrl(url, partPath, onBytes, /*allowResume=*/true)) {
        SF_LOG_E("RomM", "Failed to download from %s: %s", url.c_str(), client_.lastError().c_str());
        return false;
    }

    // Verify file was downloaded
    if (!FileSystem::exists(partPath)) {
        SF_LOG_E("RomM", "Downloaded file not found at %s", partPath.c_str());
        return false;
    }

    // Integrity: extraction verifies CRCs, but a single-file download has no such check. If the
    // wire reported the true size and the file on disk differs, the partial bytes came from a
    // stale or changed server copy — drop them so the next attempt starts from byte 0.
    const int64_t finalSize = static_cast<int64_t>(FileSystem::fileSize(partPath));
    if (observedContentTotal > 0 && finalSize != observedContentTotal) {
        SF_LOG_E("RomM", "Size mismatch on %s (%lld bytes vs %lld); partial bytes discarded",
                 partPath.c_str(), static_cast<long long>(finalSize),
                 static_cast<long long>(observedContentTotal));
        FileSystem::removeFile(partPath);
        return false;
    }

    // Atomic rename (overwrite, so re-downloading an existing ROM works)
    FileSystem::createDirectories(FileSystem::parentPath(destPath));
    if (!FileSystem::renameFileOverwrite(partPath, destPath)) {
        SF_LOG_E("RomM", "Failed to rename %s to %s", partPath.c_str(), destPath.c_str());
        FileSystem::removeFile(partPath);
        return false;
    }

    return true;
}

} // namespace sf::romm
