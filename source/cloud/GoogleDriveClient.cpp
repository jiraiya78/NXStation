#include "cloud/GoogleDriveClient.hpp"
#include "app/GoogleOAuthDefaults.hpp"
#include "app/Config.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace sf::cloud {

namespace {

constexpr const char* kOAuthTokenUrl = "https://oauth2.googleapis.com/token";
constexpr const char* kDeviceCodeUrl = "https://oauth2.googleapis.com/device/code";
constexpr const char* kDriveFilesUrl = "https://www.googleapis.com/drive/v3/files";
constexpr const char* kDriveUploadUrl = "https://www.googleapis.com/upload/drive/v3/files";
constexpr const char* kDriveAboutUrl = "https://www.googleapis.com/drive/v2/about?fields=rootFolderId";
constexpr const char* kDriveScope = "https://www.googleapis.com/auth/drive.file";
constexpr const char* kWorkspaceName = "NXStation";
constexpr const char* kRetroArchFolder = "RetroArch";

std::string jsonStringField(const Json& obj, const std::string& key)
{
    if (!obj.contains(key))
        return {};
    const Json& v = obj[key];
    if (v.is_string())
        return v.get<std::string>();
    return {};
}

double jsonNumberField(const Json& obj, const std::string& key, double fallback = 0.0)
{
    if (!obj.contains(key) || !obj[key].is_number())
        return fallback;
    return obj[key].get<double>();
}

} // namespace

GoogleDriveClient::GoogleDriveClient()
{
    reload();
}

bool GoogleDriveClient::oauthConfigPresent() const
{
    return googleoauth::credentialsConfigured();
}

bool GoogleDriveClient::isLinked() const
{
    return !refreshToken_.empty();
}

bool GoogleDriveClient::reload()
{
    ready_ = false;
    workspaceFolderId_.clear();
    listing_.clear();
    lastError_.clear();

    if (!loadOAuthConfig())
        return false;
    if (refreshToken_.empty())
        return false;
    if (!refreshAccessToken())
        return false;
    if (!fetchRootFolderId())
        return false;
    if (!fetchListing())
        return false;

    ready_ = true;
    return true;
}

bool GoogleDriveClient::loadOAuthConfig()
{
    clientId_.clear();
    clientSecret_.clear();
    refreshToken_.clear();

    if (!googleoauth::credentialsConfigured()) {
        lastError_ = "Google OAuth not configured in build";
        return false;
    }

    clientId_ = googleoauth::kClientId;
    clientSecret_ = googleoauth::kClientSecret;

    auto readRefreshToken = [](const Json& installed) -> std::string {
        return jsonStringField(installed, "refresh_token");
    };

    try {
        if (FileSystem::exists(paths::USER_CLOUD_PATH)) {
            const auto cloud = Json::parse(FileSystem::readFile(paths::USER_CLOUD_PATH));
            if (cloud.contains("google_refresh_token") && cloud["google_refresh_token"].is_string())
                refreshToken_ = cloud["google_refresh_token"].get<std::string>();
        }
        if (refreshToken_.empty() && FileSystem::exists(paths::GOOGLE_OAUTH_PATH)) {
            const auto root = Json::parse(FileSystem::readFile(paths::GOOGLE_OAUTH_PATH));
            const Json& installed = root.contains("installed") ? root["installed"] : root;
            refreshToken_ = readRefreshToken(installed);
        }
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::saveRefreshToken()
{
    try {
        Json root = Json::object();
        if (FileSystem::exists(paths::USER_CLOUD_PATH)) {
            try {
                root = Json::parse(FileSystem::readFile(paths::USER_CLOUD_PATH));
            } catch (...) {
                root = Json::object();
            }
        }
        root["google_refresh_token"] = refreshToken_;
        const std::string lastSync = Config::instance().cloudLastSyncIso();
        if (!lastSync.empty())
            root["last_sync_iso"] = lastSync;
        if (!FileSystem::writeFile(paths::USER_CLOUD_PATH, root.dump(2))) {
            lastError_ = "Failed to save refresh token";
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::jsonHasError(const std::string& body, bool logError) const
{
    if (body.empty())
        return false;
    try {
        const auto j = Json::parse(body);
        if (!j.contains("error"))
            return false;
        const Json& err = j["error"];
        if (err.is_string())
            lastError_ = err.get<std::string>();
        else if (err.is_object() && err.contains("message") && err["message"].is_string())
            lastError_ = err["message"].get<std::string>();
        else
            lastError_ = "Google API error";
        if (logError)
            SF_LOG_W("Cloud", "Drive API error: %s", lastError_.c_str());
        return true;
    } catch (...) {
        return false;
    }
}

bool GoogleDriveClient::refreshAccessToken()
{
    if (refreshToken_.empty()) {
        lastError_ = "Not linked";
        return false;
    }

    const std::time_t now = std::time(nullptr);
    if (!accessToken_.empty() && tokenExpires_ > now + 10)
        return true;

    std::ostringstream form;
    form << "client_id=" << urlEncode(clientId_) << "&client_secret=" << urlEncode(clientSecret_)
         << "&refresh_token=" << urlEncode(refreshToken_) << "&grant_type=refresh_token";

    curl_.clearAuth();
    const CurlResponse resp = curl_.postForm(kOAuthTokenUrl, form.str());
    if (!resp.ok || jsonHasError(resp.body, true)) {
        if (lastError_.empty())
            lastError_ = resp.error.empty() ? "Token refresh failed" : resp.error;
        return false;
    }

    try {
        const auto j = Json::parse(resp.body);
        accessToken_ = jsonStringField(j, "access_token");
        const double expiresIn = jsonNumberField(j, "expires_in", 3600.0);
        if (accessToken_.empty()) {
            lastError_ = "Missing access_token";
            return false;
        }
        tokenExpires_ = now + static_cast<std::time_t>(expiresIn);
        authHeader_ = "Authorization: Bearer " + accessToken_;
        curl_.setBearerAuth(accessToken_);
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::fetchRootFolderId()
{
    const CurlResponse resp = curl_.get(kDriveAboutUrl);
    if (!resp.ok || jsonHasError(resp.body, true)) {
        if (lastError_.empty())
            lastError_ = resp.error;
        return false;
    }

    try {
        const auto j = Json::parse(resp.body);
        rootFolderId_ = jsonStringField(j, "rootFolderId");
        if (rootFolderId_.empty()) {
            lastError_ = "Missing rootFolderId";
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::fetchListing()
{
    listing_.clear();
    std::string pageToken;

    do {
        std::string url = std::string(kDriveFilesUrl) +
                          "?fields=nextPageToken,files(id,name,parents,mimeType,size)"
                          "&pageSize=200&q=trashed=false";
        if (!pageToken.empty())
            url += "&pageToken=" + urlEncode(pageToken);

        const CurlResponse resp = curl_.get(url);
        if (!resp.ok || jsonHasError(resp.body, true)) {
            if (lastError_.empty())
                lastError_ = resp.error;
            return false;
        }

        try {
            const auto j = Json::parse(resp.body);
            if (j.contains("files") && j["files"].is_array()) {
                for (const auto& file : j["files"]) {
                    DriveItem item;
                    item.id = jsonStringField(file, "id");
                    item.name = jsonStringField(file, "name");
                    item.size = static_cast<int64_t>(jsonNumberField(file, "size", 0));
                    const std::string mime = jsonStringField(file, "mimeType");
                    item.isFolder = mime == "application/vnd.google-apps.folder";
                    if (file.contains("parents") && file["parents"].is_array() && file["parents"].size() > 0)
                        item.parentId = file["parents"][0].get<std::string>();
                    if (!item.id.empty())
                        listing_.push_back(std::move(item));
                }
            }
            pageToken = jsonStringField(j, "nextPageToken");
        } catch (const std::exception& ex) {
            lastError_ = ex.what();
            return false;
        }
    } while (!pageToken.empty());

    return true;
}

std::optional<DriveItem> GoogleDriveClient::findChildByName(const std::string& parentId,
                                                          const std::string& name, bool folder) const
{
    for (const auto& item : listing_) {
        if (item.parentId == parentId && item.name == name && item.isFolder == folder)
            return item;
    }
    return std::nullopt;
}

bool GoogleDriveClient::createFolder(const std::string& name, const std::string& parentId,
                                     std::string& outId)
{
    Json body = Json::object();
    body["name"] = name;
    body["mimeType"] = "application/vnd.google-apps.folder";
    if (!parentId.empty()) {
        Json parents = Json::array();
        parents.push_back(parentId);
        body["parents"] = parents;
    }

    const CurlResponse resp = curl_.postJson(kDriveFilesUrl, body.dump());
    if (!resp.ok || jsonHasError(resp.body, true)) {
        if (lastError_.empty())
            lastError_ = resp.error;
        return false;
    }

    try {
        const auto j = Json::parse(resp.body);
        outId = jsonStringField(j, "id");
        if (outId.empty()) {
            lastError_ = "Folder create missing id";
            return false;
        }
        DriveItem item;
        item.id = outId;
        item.name = name;
        item.parentId = parentId;
        item.isFolder = true;
        listing_.push_back(std::move(item));
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::ensureFolderChain(const std::vector<std::string>& parts, std::string& outLeafId)
{
    std::string parent = rootFolderId_;
    for (const auto& part : parts) {
        if (auto existing = findChildByName(parent, part, true)) {
            parent = existing->id;
            continue;
        }
        std::string created;
        if (!createFolder(part, parent, created))
            return false;
        parent = created;
    }
    outLeafId = parent;
    return true;
}

bool GoogleDriveClient::ensureWorkspace()
{
    if (!refreshAccessToken())
        return false;
    if (!workspaceFolderId_.empty())
        return true;

    std::string leaf;
    if (!ensureFolderChain({kWorkspaceName, kRetroArchFolder}, leaf))
        return false;
    workspaceFolderId_ = leaf;
    return true;
}

bool GoogleDriveClient::requestDeviceSignIn(DeviceSignInData& out)
{
    out = {};
    if (!loadOAuthConfig())
        return false;

    std::ostringstream form;
    form << "client_id=" << urlEncode(clientId_) << "&scope=" << urlEncode(kDriveScope);

    curl_.clearAuth();
    const CurlResponse resp = curl_.postForm(kDeviceCodeUrl, form.str());
    if (!resp.ok || jsonHasError(resp.body, true)) {
        if (lastError_.empty())
            lastError_ = resp.error;
        return false;
    }

    try {
        const auto j = Json::parse(resp.body);
        const std::string verificationUrl = jsonStringField(j, "verification_url");
        const std::string userCode = jsonStringField(j, "user_code");
        out.deviceCode = jsonStringField(j, "device_code");
        std::string completeUrl = jsonStringField(j, "verification_uri_complete");
        const double expiresIn = jsonNumberField(j, "expires_in", 600.0);
        out.pollIntervalSeconds = static_cast<int>(jsonNumberField(j, "interval", 5.0));
        if (out.deviceCode.empty() || verificationUrl.empty() || userCode.empty()) {
            lastError_ = "Malformed device code response";
            return false;
        }
        out.userCode = userCode;
        if (completeUrl.empty()) {
            completeUrl = verificationUrl;
            if (completeUrl.find('?') == std::string::npos)
                completeUrl += "?user_code=" + urlEncode(userCode);
            else
                completeUrl += "&user_code=" + urlEncode(userCode);
        }
        out.qrUrl = completeUrl;
        out.expiresAt = std::time(nullptr) + static_cast<std::time_t>(expiresIn);
        out.message = "Scan the QR code or go to google.com/device";
        return true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::pollDeviceSignIn(const std::string& deviceCode)
{
    std::ostringstream form;
    form << "client_id=" << urlEncode(clientId_) << "&client_secret=" << urlEncode(clientSecret_)
         << "&device_code=" << urlEncode(deviceCode)
         << "&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code";

    curl_.clearAuth();
    const CurlResponse resp = curl_.postForm(kOAuthTokenUrl, form.str());
    if (!resp.ok) {
        if (resp.body.find("authorization_pending") != std::string::npos ||
            resp.body.find("slow_down") != std::string::npos)
            return false;
        jsonHasError(resp.body, true);
        if (lastError_.empty())
            lastError_ = resp.error;
        return false;
    }

    try {
        const auto j = Json::parse(resp.body);
        accessToken_ = jsonStringField(j, "access_token");
        refreshToken_ = jsonStringField(j, "refresh_token");
        const double expiresIn = jsonNumberField(j, "expires_in", 3600.0);
        if (accessToken_.empty() || refreshToken_.empty()) {
            lastError_ = "Missing tokens in sign-in response";
            return false;
        }
        tokenExpires_ = std::time(nullptr) + static_cast<std::time_t>(expiresIn);
        curl_.setBearerAuth(accessToken_);
        if (!saveRefreshToken())
            return false;
        return reload();
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        return false;
    }
}

bool GoogleDriveClient::uploadOrPatchFile(const std::string& localPath, const std::string& remoteFileName)
{
    if (!ensureWorkspace())
        return false;

    std::optional<DriveItem> existing =
        findChildByName(workspaceFolderId_, remoteFileName, false);

    if (existing) {
        std::string url = std::string(kDriveUploadUrl) + "/" + existing->id + "?uploadType=resumable";
        const CurlResponse patchResp = curl_.patchCaptureHeaders(url);
        auto locIt = patchResp.headers.find("Location");
        if (locIt == patchResp.headers.end()) {
            lastError_ = "Missing upload Location header (patch)";
            return false;
        }
        const CurlResponse uploadResp = curl_.uploadFromFile(locIt->second, localPath);
        if (!uploadResp.ok) {
            lastError_ = uploadResp.error.empty() ? "Upload failed" : uploadResp.error;
            return false;
        }
        return true;
    }

    Json meta = Json::object();
    meta["name"] = remoteFileName;
    Json parents = Json::array();
    parents.push_back(workspaceFolderId_);
    meta["parents"] = parents;

    const std::string initUrl = std::string(kDriveUploadUrl) + "?uploadType=resumable";
    const CurlResponse initResp = curl_.postJsonCaptureHeaders(initUrl, meta.dump());
    auto locIt = initResp.headers.find("Location");
    if (locIt == initResp.headers.end()) {
        lastError_ = "Missing upload Location header";
        return false;
    }

    const CurlResponse uploadResp = curl_.uploadFromFile(locIt->second, localPath);
    if (!uploadResp.ok) {
        lastError_ = uploadResp.error.empty() ? "Upload failed" : uploadResp.error;
        return false;
    }

    if (!fetchListing())
        SF_LOG_W("Cloud", "Upload OK but listing refresh failed");

    return true;
}

bool GoogleDriveClient::listBackupArchives(std::vector<DriveItem>& out)
{
    out.clear();
    if (!ensureWorkspace())
        return false;

    for (const auto& item : listing_) {
        if (item.isFolder || item.parentId != workspaceFolderId_)
            continue;
        if (item.name.rfind("retroarch-saves-", 0) != 0)
            continue;
        if (item.name.size() < 5 || item.name.compare(item.name.size() - 4, 4, ".zip") != 0)
            continue;
        out.push_back(item);
    }

    std::sort(out.begin(), out.end(),
              [](const DriveItem& a, const DriveItem& b) { return a.name > b.name; });
    return true;
}

bool GoogleDriveClient::downloadFileById(const std::string& fileId, const std::string& localPath)
{
    if (!refreshAccessToken())
        return false;

    const std::string url = std::string(kDriveFilesUrl) + "/" + fileId + "?alt=media";
    const CurlResponse resp = curl_.downloadToFile(url, localPath);
    if (!resp.ok) {
        lastError_ = resp.error.empty() ? "Download failed" : resp.error;
        return false;
    }
    return true;
}

std::string GoogleDriveClient::urlEncode(const std::string& value) const
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

} // namespace sf::cloud
