#pragma once

#include <curl/curl.h>
#include <map>
#include <string>
#include <vector>

namespace sf::cloud {

struct CurlResponse {
    bool ok = false;
    long status = 0;
    std::string body;
    std::string error;
    std::map<std::string, std::string> headers;
};

/** Blocking libcurl helper for cloud OAuth / Drive API (worker thread only). */
class CurlHelper {
public:
    CurlHelper();
    ~CurlHelper();

    CurlHelper(const CurlHelper&) = delete;
    CurlHelper& operator=(const CurlHelper&) = delete;

    void reset();
    void setBearerAuth(const std::string& token);
    void clearAuth();

    CurlResponse get(const std::string& url);
    CurlResponse postForm(const std::string& url, const std::string& formBody);
    CurlResponse postJson(const std::string& url, const std::string& jsonBody);
  CurlResponse postJsonCaptureHeaders(const std::string& url, const std::string& jsonBody);
    CurlResponse patchCaptureHeaders(const std::string& url);
    CurlResponse uploadFromFile(const std::string& url, const std::string& filePath);
    CurlResponse downloadToFile(const std::string& url, const std::string& filePath);
    CurlResponse deleteUrl(const std::string& url);

private:
    static size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeFile(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t readFile(char* ptr, size_t size, size_t nmemb, void* userdata);

    CurlResponse perform(const char* method, const std::string& url, const std::string* body,
                         bool captureHeaders, bool uploadFile, const std::string* filePath);

    CURL* curl_ = nullptr;
    std::string authHeader_;
    std::string uploadPath_;
};

} // namespace sf::cloud
