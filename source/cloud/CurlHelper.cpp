#include "cloud/CurlHelper.hpp"
#include "app/Version.hpp"

#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include "util/FileSystem.hpp"

namespace sf::cloud {

namespace {

std::string userAgent()
{
    return std::string("NXStation/") + kAppVersion;
}

std::string trimHeaderLine(std::string line)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
        line.pop_back();
    return line;
}

} // namespace

CurlHelper::CurlHelper()
{
    curl_ = curl_easy_init();
}

CurlHelper::~CurlHelper()
{
    if (curl_)
        curl_easy_cleanup(curl_);
}

void CurlHelper::reset()
{
    if (!curl_)
        return;
    curl_easy_reset(curl_);
    const std::string ua = userAgent();
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, ua.c_str());
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
#ifdef __SWITCH__
    curl_easy_setopt(curl_, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#endif
}

void CurlHelper::setBearerAuth(const std::string& token)
{
    authHeader_ = "Authorization: Bearer " + token;
}

void CurlHelper::clearAuth()
{
    authHeader_.clear();
}

size_t CurlHelper::writeBody(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t CurlHelper::writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(ptr, size * nmemb);
    line = trimHeaderLine(line);
    const auto colon = line.find(':');
    if (colon == std::string::npos || line.empty())
        return size * nmemb;
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && value.front() == ' ')
        value.erase(value.begin());
    (*headers)[key] = value;
    return size * nmemb;
}

size_t CurlHelper::readFile(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* in = static_cast<std::ifstream*>(userdata);
    in->read(ptr, static_cast<std::streamsize>(size * nmemb));
    return static_cast<size_t>(in->gcount());
}

CurlResponse CurlHelper::perform(const char* method, const std::string& url, const std::string* body,
                                 bool captureHeaders, bool uploadFile, const std::string* filePath)
{
    CurlResponse resp;
    if (!curl_) {
        resp.error = "curl not initialized";
        return resp;
    }

    reset();
    curl_slist* hdr = nullptr;
    auto appendHeader = [&](const std::string& line) {
        hdr = curl_slist_append(hdr, line.c_str());
    };

    if (!authHeader_.empty())
        appendHeader(authHeader_);

    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHelper::writeBody);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseBody);
    if (captureHeaders) {
        curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &CurlHelper::writeHeader);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &responseHeaders);
    }

    if (method && method[0] != '\0')
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method);

    std::ifstream uploadStream;
    if (uploadFile && filePath) {
        uploadStream.open(*filePath, std::ios::binary);
        if (!uploadStream) {
            resp.error = "Cannot open upload file";
            if (hdr)
                curl_slist_free_all(hdr);
            return resp;
        }
        curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl_, CURLOPT_READFUNCTION, &CurlHelper::readFile);
        curl_easy_setopt(curl_, CURLOPT_READDATA, &uploadStream);
        struct stat st {};
        if (stat(filePath->c_str(), &st) == 0)
            curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(st.st_size));
    } else if (body) {
        appendHeader("Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
    }

    if (hdr)
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, hdr);

    const CURLcode code = curl_easy_perform(curl_);
    if (hdr)
        curl_slist_free_all(hdr);

    resp.body = std::move(responseBody);
    resp.headers = std::move(responseHeaders);
    if (code != CURLE_OK) {
        resp.error = curl_easy_strerror(code);
        return resp;
    }

    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &resp.status);
    resp.ok = resp.status >= 200 && resp.status < 300;
    if (!resp.ok && resp.error.empty())
        resp.error = "HTTP " + std::to_string(resp.status);
    return resp;
}

CurlResponse CurlHelper::get(const std::string& url)
{
    return perform(nullptr, url, nullptr, false, false, nullptr);
}

CurlResponse CurlHelper::postForm(const std::string& url, const std::string& formBody)
{
    return perform("POST", url, &formBody, false, false, nullptr);
}

CurlResponse CurlHelper::postJson(const std::string& url, const std::string& jsonBody)
{
    CurlResponse resp;
    if (!curl_) {
        resp.error = "curl not initialized";
        return resp;
    }

    reset();
    curl_slist* hdr = nullptr;
    hdr = curl_slist_append(hdr, "Content-Type: application/json");
    if (!authHeader_.empty())
        hdr = curl_slist_append(hdr, authHeader_.c_str());

    std::string responseBody;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHelper::writeBody);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseBody);

    const CURLcode code = curl_easy_perform(curl_);
    curl_slist_free_all(hdr);

    resp.body = std::move(responseBody);
    if (code != CURLE_OK) {
        resp.error = curl_easy_strerror(code);
        return resp;
    }
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &resp.status);
    resp.ok = resp.status >= 200 && resp.status < 300;
    if (!resp.ok && resp.error.empty())
        resp.error = "HTTP " + std::to_string(resp.status);
    return resp;
}

CurlResponse CurlHelper::postJsonCaptureHeaders(const std::string& url, const std::string& jsonBody)
{
    CurlResponse resp;
    if (!curl_) {
        resp.error = "curl not initialized";
        return resp;
    }

    reset();
    curl_slist* hdr = nullptr;
    hdr = curl_slist_append(hdr, "Content-Type: application/json");
    if (!authHeader_.empty())
        hdr = curl_slist_append(hdr, authHeader_.c_str());

    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHelper::writeBody);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &CurlHelper::writeHeader);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &responseHeaders);

    const CURLcode code = curl_easy_perform(curl_);
    curl_slist_free_all(hdr);

    resp.body = std::move(responseBody);
    resp.headers = std::move(responseHeaders);
    if (code != CURLE_OK) {
        resp.error = curl_easy_strerror(code);
        return resp;
    }
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &resp.status);
    resp.ok = resp.status >= 200 && resp.status < 300;
    if (!resp.ok && resp.error.empty())
        resp.error = "HTTP " + std::to_string(resp.status);
    return resp;
}

CurlResponse CurlHelper::patchCaptureHeaders(const std::string& url)
{
    return perform("PATCH", url, nullptr, true, false, nullptr);
}

CurlResponse CurlHelper::uploadFromFile(const std::string& url, const std::string& filePath)
{
    return perform(nullptr, url, nullptr, false, true, &filePath);
}

CurlResponse CurlHelper::downloadToFile(const std::string& url, const std::string& filePath)
{
    CurlResponse resp;
    if (!curl_) {
        resp.error = "curl not initialized";
        return resp;
    }

    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        resp.error = "Cannot open output file";
        return resp;
    }

    reset();
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 600L);

    curl_slist* hdr = nullptr;
    if (!authHeader_.empty())
        hdr = curl_slist_append(hdr, authHeader_.c_str());

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHelper::writeFile);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &out);
    if (hdr)
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, hdr);

    const CURLcode code = curl_easy_perform(curl_);
    if (hdr)
        curl_slist_free_all(hdr);
    out.close();

    if (code != CURLE_OK) {
        resp.error = curl_easy_strerror(code);
        FileSystem::removeFile(filePath);
        return resp;
    }

    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &resp.status);
    resp.ok = resp.status >= 200 && resp.status < 300;
    if (!resp.ok && resp.error.empty())
        resp.error = "HTTP " + std::to_string(resp.status);
    if (!resp.ok)
        FileSystem::removeFile(filePath);
    return resp;
}

size_t CurlHelper::writeFile(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::ofstream*>(userdata);
    out->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

CurlResponse CurlHelper::deleteUrl(const std::string& url)
{
    return perform("DELETE", url, nullptr, false, false, nullptr);
}

} // namespace sf::cloud
