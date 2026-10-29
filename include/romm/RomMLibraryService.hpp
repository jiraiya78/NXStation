#pragma once

#include "romm/RomMClient.hpp"
#include "romm/RomMConfig.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <atomic>

namespace sf::romm {

/** RomM library entry with additional metadata for UI. */
struct LibraryEntry {
    RomMLibraryEntry rom;
    bool downloadedLocally = false;
    std::string localPath;      // If downloaded
    std::string matchedSystemId; // NXStation system ID if matched
};

/** Byte-level download progress for one file of a ROM. */
struct DownloadProgress {
    int fileIndex = 0;          // 1-based index of the file being downloaded
    int totalFiles = 1;
    int64_t downloadedBytes = 0;
    int64_t totalBytes = 0;     // 0 when the server does not report a content length
    std::string fileName;
    /** True when the transfer continued from bytes already on disk (a resumed .part file). */
    bool resumed = false;
};

/** Progress callback; return true to abort the download. */
using DownloadProgressFn = std::function<bool(const DownloadProgress&)>;

/** RomM library service - handles ROM browsing and downloading. */
class RomMLibraryService {
public:
    RomMLibraryService(RomMClient& client, const RomMConfig& config);

    /** Fetch the user's RomM library. */
    bool fetchLibrary(std::vector<LibraryEntry>& out,
                      std::function<void(int, int)> progress = {});

    /** Match RomM ROMs to NXStation systems. */
    void matchToSystems(std::vector<LibraryEntry>& entries);

    /** Download a ROM (handles multi-file ROMs). */
    bool downloadRom(const LibraryEntry& entry, const std::string& destDir,
                     const DownloadProgressFn& progress = {},
                     std::atomic<bool>* abort = nullptr);

    /**
     * Download selected files of a ROM. RomM returns a ZIP for multi-part ROMs, which is
     * extracted into the ROM's folder inside destDir. Empty fileIds means "all files".
     */
    bool downloadRomFiles(const LibraryEntry& entry, const std::vector<long long>& fileIds,
                          const std::string& destDir,
                          const DownloadProgressFn& progress = {},
                          std::atomic<bool>* abort = nullptr);

    /** Download a single file from a multi-file ROM. */
    bool downloadRomFile(const std::string& romId, const std::string& fileName,
                         const std::string& localPath,
                         const DownloadProgressFn& progress = {});

    /** Refresh NXStation's library after downloading ROMs. */
    void triggerLibraryRefresh();

    /** Get the configured ROM mapping file path. */
    std::string mappingFilePath() const;

    /** Load / persist the library snapshot used to populate the main page offline. */
    bool loadCache();
    bool saveCache() const;

    /** Cached library entries (populated by loadCache / fetchLibrary). */
    const std::vector<LibraryEntry>& entries() const { return entries_; }
    const LibraryEntry* entryByRomId(const std::string& romId) const;
    /** Mark an entry as no longer downloaded locally (after the local copy is deleted). */
    void markNotDownloaded(const std::string& romId);

    /** Download destination folder for a matched system: the scanner's ROM folder. */
    std::string downloadDirFor(const std::string& systemId) const;

    /** Folder a multi-file ROM is stored in on the server (RomM fs_path last segment). */
    static std::string romFolderName(const RomMLibraryEntry& rom);

private:
    RomMClient& client_;
    const RomMConfig& config_;
    std::vector<LibraryEntry> entries_;

    std::string cacheFilePath() const;
    void refreshDownloadedFlags(std::vector<LibraryEntry>& entries);

    /** Folders a downloaded ROM could be found in (scanner path, RomM download root). */
    std::vector<std::string> candidateDirsFor(const LibraryEntry& entry) const;

    /** Fill a cached entry's files from /roms/{id}/simple when the list response omitted them. */
    bool resolveFiles(LibraryEntry& entry);

    /** Match a RomM platform to NXStation system ID. */
    std::optional<std::string> matchPlatformToSystem(const std::string& platform) const;

    /** Check if ROM is already downloaded locally. */
    bool isRomDownloaded(const RomMLibraryEntry& rom, std::string& outPath) const;

    /** Atomic download: write to .part file, validate, rename (overwriting any existing copy). */
    bool downloadFileAtomic(const std::string& url, const std::string& destPath,
                            const DownloadProgress& base,
                            const DownloadProgressFn& progress,
                            std::atomic<bool>* abort);
};

} // namespace sf::romm
