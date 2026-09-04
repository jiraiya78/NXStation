#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace sf {

struct HttpResponse {
    bool ok = false;
    long status = 0;
    std::string body;
    std::string error;
};

using HttpCallback = std::function<void(HttpResponse)>;
using HttpProgressCallback = std::function<void(int percent)>;
using HttpHeaderList = std::vector<std::pair<std::string, std::string>>;

class HttpClient {
public:
    static HttpClient& instance();

    void get(const std::string& url, HttpCallback cb, HttpHeaderList headers = {});
    /** Same as download(); clearer name for updater call sites. */
    void getDownload(const std::string& url, const std::string& destPath, HttpCallback cb,
                     HttpProgressCallback progress = {});
    void download(const std::string& url, const std::string& destPath, HttpCallback cb,
                  HttpProgressCallback progress = {});

    /** Process completed callbacks on the calling (main) thread. */
    void pump();

    void setConnectTimeout(long seconds) { connectTimeout_ = seconds; }
    void setTransferTimeout(long seconds) { transferTimeout_ = seconds; }

    /** Called from the download worker (via curl xferinfo) to surface % on the UI thread. */
    void enqueueProgress(const HttpProgressCallback& cb, int percent);

private:
    HttpClient();
    ~HttpClient();

    void enqueueResult(HttpCallback cb, HttpResponse resp);
    static size_t writeString(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeFile(char* ptr, size_t size, size_t nmemb, void* userdata);

    long connectTimeout_ = 20;
    long transferTimeout_ = 45;

    std::mutex resultMutex_;
    std::queue<std::pair<HttpCallback, HttpResponse>> results_;
    std::queue<std::pair<HttpProgressCallback, int>> progress_;
};

} // namespace sf
