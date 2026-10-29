#pragma once

#include "cloud/CurlHelper.hpp"
#include "romm/RomMConfig.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace sf::romm {

/** Represents a RomM device. */
struct Device {
    std::string id;
    std::string name;
    std::string platform;
    std::string lastSeen;
};

/** Represents a save file or state in RomM. */
struct RomMSave {
    std::string id;
    std::string romId;
    std::string slot;
    std::string deviceId;
    std::string sessionId;
    std::string hash;
    int64_t size = 0;
    std::string type;      // "save" or "state"
    std::string coreId;    // For save states
    std::string createdAt;
    std::string updatedAt;
};

/** One file of a ROM, as returned by RomM's RomFileSchema. */
struct RomFileEntry {
    long long id = 0;
    std::string fileName;
    int64_t sizeBytes = 0;
    bool topLevel = true;
};

/** Represents a ROM in the RomM library (SimpleRomSchema fields). */
struct RomMLibraryEntry {
    std::string id;
    std::string name;
    /** platform_fs_slug — this is what matches NXStation system ids (e.g. "psx"). */
    std::string platform;
    std::string platformDisplay;
    std::string region;
    std::string version;
    std::vector<RomFileEntry> files; // File names for multi-file ROMs
    /** File name as stored on the server (fs_name); used when the list response omits `files`. */
    std::string fsName;
    int64_t totalSize = 0;
    /** Folder the ROM lives in on the server (fs_path); empty for loose single files. */
    std::string folderName;
    bool multiFile = false;
    /** Metadata RomM already resolved (ScreenScraper / IGDB), used for the focus panel. */
    std::string summary;
    std::string genres;
    std::string developer;
    std::string publisher;
    std::string releaseDate;
    std::string coverUrl;
    /** Sibling ROMs of a multi-disc set (e.g. The Legend of Dragoon discs). */
    std::vector<std::string> siblingNames;

    /** Convenience: plain file names, for callers that do not need the per-file id/size. */
    std::vector<std::string> fileNames() const
    {
        std::vector<std::string> names;
        names.reserve(files.size());
        for (const auto& f : files)
            names.push_back(f.fileName);
        return names;
    }
};

/** Pairing response from /api/client-tokens/exchange. */
struct PairingResponse {
    std::string clientToken;
    std::string accessToken;
    time_t expiresAt = 0;
};

/** Sync negotiation response. */
struct NegotiateResponse {
    std::string sessionId;
    std::vector<RomMSave> uploads;     // Saves to upload
    std::vector<RomMSave> downloads;   // Saves to download
    std::vector<RomMSave> conflicts;   // Conflicts requiring resolution
    std::vector<RomMSave> deletes;     // Saves to delete locally
};

/** Upload save response. */
struct UploadResponse {
    std::string saveId;
    std::string sessionId;
};

/** Result of a sync operation. */
struct SyncResult {
    bool ok = false;
    std::string message;
    size_t uploaded = 0;
    size_t downloaded = 0;
    size_t conflicts = 0;
    size_t failed = 0;
};

/** RomM HTTP client using the project's existing libcurl infrastructure. */
class RomMClient {
public:
    RomMClient();
    ~RomMClient();

    /** Initialize with server URL and credentials. */
    void configure(const std::string& serverUrl, const std::string& accessToken = "",
                   const std::string& clientToken = "");

    /** Check if client is configured and ready. */
    bool isReady() const { return ready_; }

    /** Get last error message. */
    std::string lastError() const { return lastError_; }

    // Pairing & Device Registration
    bool exchangePairingCode(const std::string& code, PairingResponse& out);
    bool registerDevice(const std::string& deviceName, Device& out);
    bool updateDevice(const std::string& deviceId, const std::string& name);

    // Save Sync Protocol
    bool negotiate(const std::vector<RomMSave>& localSaves, NegotiateResponse& out);
    bool uploadSave(const std::string& localPath, const RomMSave& meta, UploadResponse& out);
    bool downloadSave(const std::string& saveId, const std::string& localPath);
    bool confirmDownload(const std::string& saveId, const std::string& hash);
    bool completeSyncSession(const std::string& sessionId,
                             const std::vector<std::string>& completed,
                             const std::vector<std::string>& failed);

    // Library API
    bool fetchLibrary(std::vector<RomMLibraryEntry>& out,
                      const std::function<void(int fetched, int total)>& progress = {});
    /** Single ROM as SimpleRomSchema; used when the list response omits the `files` array. */
    bool fetchSimpleRom(const std::string& romId, RomMLibraryEntry& out);
    bool downloadRom(const std::string& romId, const std::string& fileName,
                     const std::string& localPath, const sf::cloud::ProgressFn& progress = {},
                     bool allowResume = false);
    bool downloadToUrl(const std::string& url, const std::string& localPath,
                       const sf::cloud::ProgressFn& progress = {}, bool allowResume = false);
    std::string buildUrlPublic(const std::string& path) const { return buildUrl(path); }

private:
    sf::cloud::CurlHelper curl_;
    std::string baseUrl_;
    std::string accessToken_;
    std::string clientToken_;
    bool ready_ = false;
    mutable std::string lastError_;

    std::string buildUrl(const std::string& path) const;
    void updateAuthHeaders();
    bool checkResponse(const sf::cloud::CurlResponse& resp, const char* operation);
};

} // namespace sf::romm
