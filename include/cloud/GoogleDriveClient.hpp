#pragma once

#include "cloud/CurlHelper.hpp"

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace sf::cloud {

struct DriveItem {
    std::string id;
    std::string name;
    std::string parentId;
    bool isFolder = false;
    int64_t size = 0;
};

struct DeviceSignInData {
    std::string message;
    std::string userCode;
    /** URL to encode in the QR (verification_uri_complete when available). */
    std::string qrUrl;
    std::string deviceCode;
    std::time_t expiresAt = 0;
    int pollIntervalSeconds = 5;
};

/** Google Drive client using OAuth device flow (JKSV-compatible config file). */
class GoogleDriveClient {
public:
    GoogleDriveClient();

    bool oauthConfigPresent() const;
    bool isLinked() const;
    bool isReady() const { return ready_; }

    bool reload();
    bool refreshAccessToken();

    bool requestDeviceSignIn(DeviceSignInData& out);
    bool pollDeviceSignIn(const std::string& deviceCode);

    bool ensureWorkspace();
    bool uploadOrPatchFile(const std::string& localPath, const std::string& remoteFileName);

    /** List NXStation backup ZIPs in the Drive workspace (newest first). */
    bool listBackupArchives(std::vector<DriveItem>& out);
    bool downloadFileById(const std::string& fileId, const std::string& localPath);

    std::string lastError() const { return lastError_; }

private:
    bool loadOAuthConfig();
    bool saveRefreshToken();
    bool fetchRootFolderId();
    bool fetchListing();
    bool createFolder(const std::string& name, const std::string& parentId, std::string& outId);
    std::optional<DriveItem> findChildByName(const std::string& parentId, const std::string& name,
                                             bool folder) const;
    bool ensureFolderChain(const std::vector<std::string>& parts, std::string& outLeafId);
    bool jsonHasError(const std::string& body, bool logError) const;
    std::string urlEncode(const std::string& value) const;

    CurlHelper curl_;
    std::string clientId_;
    std::string clientSecret_;
    std::string refreshToken_;
    std::string accessToken_;
    std::string authHeader_;
    std::time_t tokenExpires_ = 0;
    std::string rootFolderId_;
    std::string workspaceFolderId_;
    std::vector<DriveItem> listing_;
    bool ready_ = false;
    mutable std::string lastError_;
};

} // namespace sf::cloud
