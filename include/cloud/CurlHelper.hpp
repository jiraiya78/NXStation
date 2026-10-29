#pragma once

#include <curl/curl.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

namespace sf::cloud {

using ProgressFn = std::function<bool(int64_t downloaded, int64_t total)>;

struct CurlResponse {
    bool ok = false;
    long status = 0;
    std::string body;
    std::string error;
    std::map<std::string, std::string> headers;
};

/**
 * Blocking libcurl helper.
 *
 * Guarded by a mutex: one instance can be reached from several pool workers (the RomM
 * library fetch, save sync and ROM downloads all run on the pool), and a CURL easy
 * handle must not be entered from two threads at the same time — that is what produces
 * the spurious CURLE_FAILED_INIT ("Failed initialization") errors.
 *
 * curl_global_init() is performed once in main(); handles are created lazily here.
 */
class CurlHelper {
public:
    CurlHelper();
    ~CurlHelper();

    CurlHelper(const CurlHelper&) = delete;
    CurlHelper& operator=(const CurlHelper&) = delete;

    void setBearerAuth(const std::string& token);
    void clearAuth();

    /** Timeouts in seconds: normal API calls, and large ROM downloads. */
    void setTimeouts(long connectSeconds, long transferSeconds);
    void setDownloadTimeout(long seconds);

    CurlResponse get(const std::string& url);
    CurlResponse postForm(const std::string& url, const std::string& formBody);
    CurlResponse postJson(const std::string& url, const std::string& jsonBody);
  CurlResponse postJsonCaptureHeaders(const std::string& url, const std::string& jsonBody);
    CurlResponse patchCaptureHeaders(const std::string& url);
    CurlResponse uploadFromFile(const std::string& url, const std::string& filePath);
    /**
     * Download to a file. When `allowResume` is set and the destination already has bytes, the
     * request sends a Range header and appends; a dropped transfer keeps the partial file so the
     * next attempt (or the next app launch) continues from it instead of starting over.
     */
    CurlResponse downloadToFile(const std::string& url, const std::string& filePath,
                                const ProgressFn& progress = {}, bool allowResume = false);
    CurlResponse deleteUrl(const std::string& url);

private:
    void reset();

    static size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeFile(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t readFile(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int xferInfo(void* userdata, curl_off_t toDownload, curl_off_t downloaded,
                        curl_off_t dlspeed, curl_off_t ulspeed);

    CurlResponse perform(const char* method, const std::string& url, const std::string* body,
                         bool captureHeaders, bool uploadFile, const std::string* filePath);

    bool ensureHandle();
    CURL* ensureDownloadHandle();
    void applyBaseOptions(CURL* handle, long transferTimeout);

    /** Read the auth header under the API mutex, so a long download can run on its own handle. */
    std::string authHeaderCopy();

    CURL* curl_ = nullptr;
    /** ROM downloads use a separate handle + mutex: a multi-minute transfer must not block (or
     *  be blocked by) the short API calls that share this helper across pool workers. */
    CURL* downloadCurl_ = nullptr;
    std::string authHeader_;
    std::string uploadPath_;
    ProgressFn progressFn_;
    long connectTimeout_ = 30;
    long transferTimeout_ = 120;
    long downloadTimeout_ = 1800;
    mutable std::mutex mutex_;
    mutable std::mutex downloadMutex_;
};

} // namespace sf::cloud
