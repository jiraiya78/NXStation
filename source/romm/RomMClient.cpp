#include "romm/RomMClient.hpp"
#include "romm/RomMConfig.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/FileSystem.hpp"

#include <sstream>
#include <ctime>
#include <iomanip>

namespace sf::romm {

namespace {

std::string jsonStringField(const Json& obj, const std::string& key) {
    if (!obj.contains(key)) return {};
    const Json& v = obj[key];
    if (v.is_string()) return v.get<std::string>();
    return {};
}

int64_t jsonNumberField(const Json& obj, const std::string& key, int64_t fallback = 0) {
    if (!obj.contains(key) || !obj[key].is_number()) return fallback;
    return static_cast<int64_t>(obj[key].get<double>());
}

bool jsonBoolField(const Json& obj, const std::string& key, bool fallback = false) {
    if (!obj.contains(key)) return fallback;
    if (obj[key].is_boolean()) return obj[key].get<bool>();
    if (obj[key].is_number()) return obj[key].get<double>() != 0.0;
    return fallback;
}

/** Joins a JSON string array (regions / genres / developers / publishers) into one label. */
std::string joinedStringField(const Json& obj, const std::string& key) {
    if (!obj.contains(key) || !obj[key].is_array()) return {};
    std::string out;
    for (const auto& v : obj[key]) {
        if (!v.is_string()) continue;
        const std::string s = v.get<std::string>();
        if (s.empty()) continue;
        if (!out.empty()) out += ", ";
        out += s;
    }
    return out;
}

time_t parseIso8601(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) return 0;
    return std::mktime(&tm);
}

const Json* libraryPageItems(const Json& j) {
    if (j.contains("data") && j["data"].is_array()) return &j["data"];
    if (j.is_array()) return &j;
    if (j.contains("items") && j["items"].is_array()) return &j["items"];
    if (j.contains("roms") && j["roms"].is_array()) return &j["roms"];
    return nullptr;
}

RomMLibraryEntry parseLibraryEntry(const Json& item) {
    RomMLibraryEntry entry;

    // id is integer per SimpleRomSchema
    double id = jsonNumberField(item, "id", 0);
    if (id == 0) id = jsonNumberField(item, "rom_id", 0);
    if (id == 0) id = jsonNumberField(item, "romId", 0);
    entry.id = std::to_string(static_cast<long long>(id));

    entry.name = jsonStringField(item, "name");
    if (entry.name.empty()) entry.name = jsonStringField(item, "fs_name");

    // Official SimpleRomSchema: platform_fs_slug is the stable key, platform_display_name the label.
    entry.platform = jsonStringField(item, "platform_fs_slug");
    if (entry.platform.empty()) entry.platform = jsonStringField(item, "platform_slug");
    if (entry.platform.empty()) entry.platform = jsonStringField(item, "platform");
    entry.platformDisplay = jsonStringField(item, "platform_display_name");
    if (entry.platformDisplay.empty()) entry.platformDisplay = jsonStringField(item, "platform_custom_name");

    // regions is a string array in the official schema; older builds used a single "region".
    entry.region = joinedStringField(item, "regions");
    if (entry.region.empty()) entry.region = jsonStringField(item, "region");
    entry.version = jsonStringField(item, "revision");
    if (entry.version.empty()) entry.version = jsonStringField(item, "version");

    entry.totalSize = jsonNumberField(item, "fs_size_bytes", 0);
    if (entry.totalSize == 0) entry.totalSize = jsonNumberField(item, "total_size", 0);

    entry.folderName = jsonStringField(item, "fs_path");
    entry.fsName = jsonStringField(item, "fs_name");

    // RomFileSchema entries carry id / file_name / file_size_bytes / is_top_level.
    if (item.contains("files") && item["files"].is_array()) {
        for (const auto& f : item["files"]) {
            RomFileEntry file;
            if (f.is_string()) {
                file.fileName = f.get<std::string>();
            } else if (f.is_object()) {
                file.id = static_cast<long long>(jsonNumberField(f, "id", 0));
                file.fileName = jsonStringField(f, "file_name");
                file.sizeBytes = jsonNumberField(f, "file_size_bytes", 0);
                file.topLevel = jsonBoolField(f, "is_top_level", true);
            }
            if (!file.fileName.empty()) entry.files.push_back(std::move(file));
        }
    }

    if (entry.files.empty()) {
        // Some RomM builds omit `files` in the paginated response; fs_name is the stored file
        // name for single-file ROMs, which is what the content endpoint path needs.
        std::string single = entry.fsName;
        if (single.empty()) single = jsonStringField(item, "file_name");
        if (single.empty()) single = jsonStringField(item, "file");
        if (!single.empty()) {
            RomFileEntry file;
            file.fileName = single;
            file.sizeBytes = entry.totalSize;
            entry.files.push_back(std::move(file));
        }
    }

    entry.multiFile = jsonBoolField(item, "has_multiple_files", false) || entry.files.size() > 1;

    // Metadata RomM has already resolved (ScreenScraper/IGDB merge) — used for the focus panel
    // when there is no local scrape entry for a not-yet-downloaded ROM.
    entry.summary = jsonStringField(item, "summary");
    if (item.contains("metadatum") && item["metadatum"].is_object()) {
        const Json& meta = item["metadatum"];
        entry.genres = joinedStringField(meta, "genres");
        entry.developer = joinedStringField(meta, "developers");
        entry.publisher = joinedStringField(meta, "publishers");
        entry.releaseDate = jsonStringField(meta, "first_release_date");
    }
    entry.coverUrl = jsonStringField(item, "url_cover");

    if (item.contains("sibling_roms") && item["sibling_roms"].is_array()) {
        for (const auto& s : item["sibling_roms"]) {
            if (s.is_object()) {
                const std::string sibling = jsonStringField(s, "name");
                if (!sibling.empty()) entry.siblingNames.push_back(sibling);
            }
        }
    }

    return entry;
}

} // namespace

RomMClient::RomMClient() = default;

RomMClient::~RomMClient() = default;

void RomMClient::configure(const std::string& serverUrl, const std::string& accessToken,
                           const std::string& clientToken) {
    baseUrl_ = serverUrl;
    if (!baseUrl_.empty() && baseUrl_.back() == '/') {
        baseUrl_.pop_back();
    }
    accessToken_ = accessToken;
    clientToken_ = clientToken;
    updateAuthHeaders();
    ready_ = !baseUrl_.empty() && !accessToken_.empty();
    lastError_.clear();
}

std::string RomMClient::buildUrl(const std::string& path) const {
    return baseUrl_ + "/api" + path;
}

void RomMClient::updateAuthHeaders() {
    if (!accessToken_.empty()) {
        curl_.setBearerAuth(accessToken_);
    } else if (!clientToken_.empty()) {
        curl_.setBearerAuth(clientToken_);
    } else {
        curl_.clearAuth();
    }
}

bool RomMClient::checkResponse(const sf::cloud::CurlResponse& resp, const char* operation) {
    if (!resp.ok) {
        lastError_ = resp.error.empty() ? std::string(operation) + " failed: HTTP " + std::to_string(resp.status)
                                         : std::string(operation) + " failed: " + resp.error;
        if (resp.status == 401)
            lastError_ += " (token invalid or expired — re-pair in RomM settings)";
        SF_LOG_E("RomM", "%s", lastError_.c_str());
        return false;
    }
    return true;
}

bool RomMClient::exchangePairingCode(const std::string& code, PairingResponse& out) {
    // Allow pairing with just base URL configured (no auth tokens needed for initial pairing)
    if (baseUrl_.empty()) {
        lastError_ = "Client not configured: server URL not set";
        return false;
    }

    Json body = Json::object();
    body["code"] = code;

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/client-tokens/exchange"), body.dump());
    if (!checkResponse(resp, "Pairing code exchange")) return false;

    try {
        const auto j = Json::parse(resp.body);
        
        // Debug: log the response
        SF_LOG_I("RomM", "Pairing response: %s", resp.body.c_str());
        
        // According to user, the token field is called raw_token
        out.accessToken = jsonStringField(j, "raw_token");
        if (out.accessToken.empty()) out.accessToken = jsonStringField(j, "rawToken");
        if (out.accessToken.empty()) out.accessToken = jsonStringField(j, "access_token");
        if (out.accessToken.empty()) out.accessToken = jsonStringField(j, "accessToken");
        if (out.accessToken.empty()) out.accessToken = jsonStringField(j, "token");
        
        // The pairing response doesn't have a separate client_token - we use the access_token
        out.clientToken = out.accessToken;
        
        double expiresIn = jsonNumberField(j, "expires", 0);
        if (expiresIn == 0) expiresIn = jsonNumberField(j, "expires_in", 0);
        if (expiresIn == 0) expiresIn = jsonNumberField(j, "expiresIn", 0);
        if (expiresIn == 0) expiresIn = 3600;
        out.expiresAt = std::time(nullptr) + static_cast<time_t>(expiresIn);

        if (out.accessToken.empty()) {
            lastError_ = "Missing raw_token in pairing response: " + resp.body;
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool RomMClient::registerDevice(const std::string& deviceName, Device& out) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    Json body = Json::object();
    body["name"] = deviceName;
    body["platform"] = "switch";

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/devices"), body.dump());
    if (!checkResponse(resp, "Device registration")) return false;

    try {
        const auto j = Json::parse(resp.body);
        SF_LOG_I("RomM", "Device registration response: %s", resp.body.c_str());
        
        auto getId = [&](const Json& obj, const std::initializer_list<const char*>& keys) -> std::string {
            for (const char* key : keys) {
                std::string val = jsonStringField(obj, key);
                if (!val.empty()) return val;
            }
            return "";
        };
        
        out.id = getId(j, {"id", "device_id", "deviceId", "uuid"});
        out.name = jsonStringField(j, "name");
        out.platform = jsonStringField(j, "platform");
        out.lastSeen = jsonStringField(j, "last_seen");
        if (out.lastSeen.empty()) out.lastSeen = jsonStringField(j, "lastSeen");

        if (out.id.empty()) {
            lastError_ = "Missing device ID in registration response: " + resp.body;
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool RomMClient::updateDevice(const std::string& deviceId, const std::string& name) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    Json body = Json::object();
    body["name"] = name;

    const sf::cloud::CurlResponse resp = curl_.patchCaptureHeaders(buildUrl("/devices/" + deviceId));
    if (!resp.ok) {
        // Try with JSON body
        const sf::cloud::CurlResponse resp2 = curl_.postJson(buildUrl("/devices/" + deviceId), body.dump());
        if (!checkResponse(resp2, "Device update")) return false;
    }
    return true;
}

bool RomMClient::negotiate(const std::vector<RomMSave>& localSaves, NegotiateResponse& out) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    Json body = Json::object();
    Json savesArray = Json::array();
    for (const auto& save : localSaves) {
        Json s = Json::object();
        s["rom_id"] = save.romId;
        s["slot"] = save.slot;
        s["device_id"] = save.deviceId;
        s["type"] = save.type;
        s["hash"] = save.hash;
        s["size"] = save.size;
        if (!save.coreId.empty()) s["core_id"] = save.coreId;
        savesArray.push_back(std::move(s));
    }
    body["saves"] = std::move(savesArray);

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/sync/negotiate"), body.dump());
    if (!checkResponse(resp, "Sync negotiate")) return false;

    try {
        const auto j = Json::parse(resp.body);
        out.sessionId = jsonStringField(j, "session_id");

        auto parseSaves = [&](const Json& arr, std::vector<RomMSave>& vec) {
            if (!arr.is_array()) return;
            for (const auto& s : arr) {
                RomMSave save;
                save.id = jsonStringField(s, "id");
                save.romId = jsonStringField(s, "rom_id");
                save.slot = jsonStringField(s, "slot");
                save.deviceId = jsonStringField(s, "device_id");
                save.sessionId = jsonStringField(s, "session_id");
                save.hash = jsonStringField(s, "hash");
                save.size = jsonNumberField(s, "size", 0);
                save.type = jsonStringField(s, "type");
                save.coreId = jsonStringField(s, "core_id");
                save.createdAt = jsonStringField(s, "created_at");
                save.updatedAt = jsonStringField(s, "updated_at");
                if (!save.id.empty()) vec.push_back(std::move(save));
            }
        };

        parseSaves(j["uploads"], out.uploads);
        parseSaves(j["downloads"], out.downloads);
        parseSaves(j["conflicts"], out.conflicts);
        parseSaves(j["deletes"], out.deletes);

        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool RomMClient::uploadSave(const std::string& localPath, const RomMSave& meta, UploadResponse& out) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    // First, create the save metadata
    Json body = Json::object();
    body["rom_id"] = meta.romId;
    body["slot"] = meta.slot;
    body["device_id"] = meta.deviceId;
    body["session_id"] = meta.sessionId;
    body["type"] = meta.type;
    body["hash"] = meta.hash;
    body["size"] = meta.size;
    if (!meta.coreId.empty()) body["core_id"] = meta.coreId;

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/saves"), body.dump());
    if (!checkResponse(resp, "Save create")) return false;

    try {
        const auto j = Json::parse(resp.body);
        std::string saveId = jsonStringField(j, "id");
        out.sessionId = jsonStringField(j, "session_id");

        if (saveId.empty()) {
            lastError_ = "Missing save ID in create response";
            return false;
        }

        // Now upload the content
        std::string uploadUrl = buildUrl("/saves/" + saveId + "/content");
        const sf::cloud::CurlResponse uploadResp = curl_.uploadFromFile(uploadUrl, localPath);
        if (!checkResponse(uploadResp, "Save content upload")) return false;

        out.saveId = saveId;
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool RomMClient::downloadSave(const std::string& saveId, const std::string& localPath) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    std::string url = buildUrl("/saves/" + saveId + "/content");
    const sf::cloud::CurlResponse resp = curl_.downloadToFile(url, localPath);
    return checkResponse(resp, "Save download");
}

bool RomMClient::confirmDownload(const std::string& saveId, const std::string& hash) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    Json body = Json::object();
    body["hash"] = hash;

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/saves/" + saveId + "/downloaded"), body.dump());
    return checkResponse(resp, "Confirm download");
}

bool RomMClient::completeSyncSession(const std::string& sessionId,
                                     const std::vector<std::string>& completed,
                                     const std::vector<std::string>& failed) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    Json body = Json::object();
    Json completedArr = Json::array();
    for (const auto& id : completed) completedArr.push_back(id);
    Json failedArr = Json::array();
    for (const auto& id : failed) failedArr.push_back(id);
    body["completed"] = std::move(completedArr);
    body["failed"] = std::move(failedArr);

    const sf::cloud::CurlResponse resp = curl_.postJson(buildUrl("/sync/sessions/" + sessionId + "/complete"), body.dump());
    return checkResponse(resp, "Complete sync session");
}

bool RomMClient::fetchLibrary(std::vector<RomMLibraryEntry>& out,
                              const std::function<void(int fetched, int total)>& progress) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    // RomM paginates with limit/offset (the default page is 50 rows), so walk every page.
    constexpr int kPageLimit = 100;
    constexpr int kMaxPages = 100;

    int offset = 0;
    int total = -1;
    bool parsed = false;

    for (int page = 0; page < kMaxPages; ++page) {
        const std::string url = buildUrl("/roms?limit=" + std::to_string(kPageLimit) +
                                         "&offset=" + std::to_string(offset));
        const sf::cloud::CurlResponse resp = curl_.get(url);
        if (!checkResponse(resp, "Fetch library")) return false;

        Json j;
        try {
            j = Json::parse(resp.body);
        } catch (const std::exception& ex) {
            lastError_ = ex.what();
            return false;
        }

        const Json* items = libraryPageItems(j);
        if (!items) {
            lastError_ = "Invalid library response format";
            return false;
        }
        parsed = true;

        const int pageCount = static_cast<int>(items->size());
        if (total < 0) {
            if (j.contains("meta") && j["meta"].is_object())
                total = static_cast<int>(jsonNumberField(j["meta"], "total", 0));
            if (total <= 0)
                total = static_cast<int>(jsonNumberField(j, "total", 0));
        }

        for (const auto& item : *items) {
            RomMLibraryEntry entry = parseLibraryEntry(item);
            if (!entry.id.empty())
                out.push_back(std::move(entry));
        }

        SF_LOG_I("RomM", "Library page %d: %d entries (server total %d)", page + 1, pageCount, total);
        if (progress)
            progress(static_cast<int>(out.size()), total > 0 ? total : static_cast<int>(out.size()));

        if (pageCount == 0) break;
        offset += pageCount;
        if (total > 0 && static_cast<int>(out.size()) >= total) break;
        // A short page only means "last page" when the server did not report a total
        // (some builds cap the page size below the requested limit).
        if (total <= 0 && pageCount < kPageLimit) break;
    }

    return parsed;
}

bool RomMClient::fetchSimpleRom(const std::string& romId, RomMLibraryEntry& out) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    // SimpleRomSchema is the documented shape that carries `files`; DetailedRomSchema does too,
    // so try the plain rom endpoint as a fallback for builds without /simple.
    const std::vector<std::string> paths = {"/roms/" + romId + "/simple", "/roms/" + romId};
    bool parsedAny = false;

    for (const std::string& path : paths) {
        const sf::cloud::CurlResponse resp = curl_.get(buildUrl(path));
        if (!checkResponse(resp, "Fetch rom")) continue;

        try {
            const Json j = Json::parse(resp.body);
            RomMLibraryEntry entry = parseLibraryEntry(j);
            if (entry.id.empty()) continue;
            parsedAny = true;
            if (!entry.files.empty()) {
                out = std::move(entry);
                return true;
            }
            if (out.id.empty()) out = std::move(entry);
        } catch (const std::exception& ex) {
            lastError_ = ex.what();
        }
    }

    return parsedAny;
}

bool RomMClient::downloadRom(const std::string& romId, const std::string& fileName,
                             const std::string& localPath, const sf::cloud::ProgressFn& progress,
                             bool allowResume) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    std::string url = buildUrl("/roms/" + romId + "/content/" + fileName);
    const sf::cloud::CurlResponse resp = curl_.downloadToFile(url, localPath, progress, allowResume);
    return checkResponse(resp, "ROM download");
}

bool RomMClient::downloadToUrl(const std::string& url, const std::string& localPath,
                               const sf::cloud::ProgressFn& progress, bool allowResume) {
    if (!ready_) {
        lastError_ = "Client not authenticated";
        return false;
    }

    const sf::cloud::CurlResponse resp = curl_.downloadToFile(url, localPath, progress, allowResume);
    return checkResponse(resp, "URL download");
}

} // namespace sf::romm
