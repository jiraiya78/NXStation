#include "scraper/HttpClient.hpp"
#include "app/AppState.hpp"
#include "app/Version.hpp"
#include "util/Logger.hpp"

#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sys/stat.h>
#include <thread>

namespace sf {

namespace {

std::string userAgent()
{
    return std::string("NXStation/") + kAppVersion;
}

bool shouldRetryHttp(CURLcode code)
{
    return code == CURLE_COULDNT_RESOLVE_HOST || code == CURLE_COULDNT_CONNECT ||
           code == CURLE_OPERATION_TIMEDOUT || code == CURLE_GOT_NOTHING;
}

CURLcode performWithRetry(CURL* curl)
{
    CURLcode code = CURLE_OK;
    for (int attempt = 0; attempt < 3; ++attempt) {
        code = curl_easy_perform(curl);
        if (code == CURLE_OK || !shouldRetryHttp(code))
            return code;
        SF_LOG_W("HTTP", "Retry %d/3 after %s", attempt + 1, curl_easy_strerror(code));
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    return code;
}

struct DownloadProgressState {
    HttpClient* client = nullptr;
    HttpProgressCallback cb;
    std::atomic<int> lastPercent{-1};
};

int xferInfo(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{
    auto* st = static_cast<DownloadProgressState*>(clientp);
    if (!st || !st->client || !st->cb)
        return 0;
    if (dltotal <= 0)
        return 0;

    int percent = static_cast<int>((dlnow * 100) / dltotal);
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    const int prev = st->lastPercent.load();
    if (percent == prev || (percent < 100 && percent < prev + 1))
        return 0;

    st->lastPercent.store(percent);
    st->client->enqueueProgress(st->cb, percent);
    return 0;
}

} // namespace

HttpClient& HttpClient::instance()
{
    static HttpClient client;
    return client;
}

HttpClient::HttpClient()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient()
{
    curl_global_cleanup();
}

size_t HttpClient::writeString(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t HttpClient::writeFile(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::ofstream*>(userdata);
    out->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

void HttpClient::enqueueResult(HttpCallback cb, HttpResponse resp)
{
    std::lock_guard<std::mutex> lock(resultMutex_);
    results_.emplace(std::move(cb), std::move(resp));
}

void HttpClient::enqueueProgress(const HttpProgressCallback& cb, int percent)
{
    if (!cb)
        return;
    std::lock_guard<std::mutex> lock(resultMutex_);
    progress_.emplace(cb, percent);
}

void HttpClient::get(const std::string& url, HttpCallback cb, HttpHeaderList headers)
{
    AppState::instance().pool().enqueue(
        [this, url, cb = std::move(cb), headers = std::move(headers)]() mutable {
            HttpResponse resp;
            CURL* curl = curl_easy_init();
            if (!curl) {
                resp.error = "curl_easy_init failed";
                enqueueResult(std::move(cb), std::move(resp));
                return;
            }

            curl_slist* hdr = nullptr;
            for (const auto& [key, value] : headers) {
                const std::string line = key + ": " + value;
                hdr = curl_slist_append(hdr, line.c_str());
            }

            const std::string ua = userAgent();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout_);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, transferTimeout_);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
#ifdef __SWITCH__
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#endif
            if (hdr)
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HttpClient::writeString);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

            CURLcode code = performWithRetry(curl);
            if (code != CURLE_OK) {
                resp.error = curl_easy_strerror(code);
                SF_LOG_W("HTTP", "GET failed: %s (%s)", url.c_str(), resp.error.c_str());
            } else {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
                resp.ok = (resp.status >= 200 && resp.status < 300);
                if (!resp.ok)
                    resp.error = "HTTP " + std::to_string(resp.status);
            }
            if (hdr)
                curl_slist_free_all(hdr);
            curl_easy_cleanup(curl);
            enqueueResult(std::move(cb), std::move(resp));
        });
}

void HttpClient::getDownload(const std::string& url, const std::string& destPath, HttpCallback cb,
                             HttpProgressCallback progress)
{
    download(url, destPath, std::move(cb), std::move(progress));
}

void HttpClient::download(const std::string& url, const std::string& destPath, HttpCallback cb,
                          HttpProgressCallback progress)
{
    AppState::instance().pool().enqueue(
        [this, url, destPath, cb = std::move(cb), progress = std::move(progress)]() mutable {
            HttpResponse resp;
            CURL* curl = curl_easy_init();
            if (!curl) {
                resp.error = "curl_easy_init failed";
                SF_LOG_E("HTTP", "Download init failed for %s", url.c_str());
                Logger::instance().flush();
                enqueueResult(std::move(cb), std::move(resp));
                return;
            }

            std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
            if (!out) {
                resp.error = "Cannot open " + destPath;
                SF_LOG_E("HTTP", "Download cannot open %s", destPath.c_str());
                Logger::instance().flush();
                curl_easy_cleanup(curl);
                enqueueResult(std::move(cb), std::move(resp));
                return;
            }

            SF_LOG_I("HTTP", "Download start: %s → %s", url.c_str(), destPath.c_str());

            DownloadProgressState progressState;
            progressState.client = this;
            progressState.cb = progress;

            const std::string ua = userAgent();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout_);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, transferTimeout_);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
#ifdef __SWITCH__
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#endif
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HttpClient::writeFile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
            if (progress) {
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfo);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressState);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            }

            CURLcode code = performWithRetry(curl);
            out.close();

            char* effectiveUrl = nullptr;
            curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
            const std::string effective =
                effectiveUrl ? std::string(effectiveUrl) : std::string();

            double curlBytes = 0.0;
            curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &curlBytes);

            struct stat st {};
            const bool hasStat = stat(destPath.c_str(), &st) == 0;
            const uint64_t fileBytes =
                hasStat ? static_cast<uint64_t>(st.st_size) : 0;

            if (code != CURLE_OK) {
                resp.error = curl_easy_strerror(code);
                SF_LOG_W("HTTP", "Download failed: %s (url=%s effective=%s file=%llu curl=%.0f)",
                         resp.error.c_str(), url.c_str(), effective.c_str(),
                         static_cast<unsigned long long>(fileBytes), curlBytes);
            } else {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
                resp.ok = (resp.status >= 200 && resp.status < 300);
                if (!resp.ok)
                    resp.error = "HTTP " + std::to_string(resp.status);

                if (resp.ok) {
                    if (progress)
                        enqueueProgress(progress, 100);
                    SF_LOG_I("HTTP",
                             "Download OK: HTTP %ld %llu bytes → %s (effective=%s)",
                             resp.status, static_cast<unsigned long long>(fileBytes),
                             destPath.c_str(), effective.c_str());
                    Logger::instance().flush();
                } else {
                    SF_LOG_W("HTTP",
                             "Download HTTP error %ld (%llu bytes, effective=%s): %s",
                             resp.status, static_cast<unsigned long long>(fileBytes),
                             effective.c_str(), resp.error.c_str());
                }
            }
            curl_easy_cleanup(curl);
            enqueueResult(std::move(cb), std::move(resp));
        });
}

void HttpClient::pump()
{
    std::queue<std::pair<HttpProgressCallback, int>> localProgress;
    std::queue<std::pair<HttpCallback, HttpResponse>> local;
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        localProgress.swap(progress_);
        local.swap(results_);
    }
    while (!localProgress.empty()) {
        auto& [cb, percent] = localProgress.front();
        if (cb)
            cb(percent);
        localProgress.pop();
    }
    while (!local.empty()) {
        auto& [cb, resp] = local.front();
        if (cb)
            cb(std::move(resp));
        local.pop();
    }
}

} // namespace sf
