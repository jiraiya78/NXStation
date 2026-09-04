#include "scraper/ScraperService.hpp"

#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "app/ScreenScraperCredentials.hpp"

#include "scraper/HttpClient.hpp"

#include "scraper/MetadataCache.hpp"
#include "scraper/ImageOptimizer.hpp"
#include "media/ManualPages.hpp"

#include "util/FileSystem.hpp"

#include "util/Hash.hpp"

#include "util/Logger.hpp"
#include "util/Paths.hpp"
#include "util/ScrapeLog.hpp"

#include "util/Network.hpp"



#include <borealis.hpp>
#include <chrono>
#include <condition_variable>
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <initializer_list>

#include <fstream>

#include <sstream>

#include <thread>



#include "util/Utf8.hpp"
#include "util/Json.hpp"



namespace sf {



namespace {



std::string scrapeCredentialError()
{
    if (!screenscraper::devCredentialsConfigured())
        return "ScreenScraper developer API keys are not configured in this build.";
    if (!Config::instance().hasScreenScraperWebsiteLogin())
        return "ScreenScraper website login required — set ssid/sspassword in Settings.";
    return {};
}

std::string urlEncode(const std::string& value)

{

    char* enc = curl_easy_escape(nullptr, value.c_str(), static_cast<int>(value.size()));

    if (!enc)

        return value;

    std::string out(enc);

    curl_free(enc);

    return out;

}



std::string maskUrl(const std::string& url)

{

    std::string out = url;

    const char* keys[] = {"devpassword=", "sspassword="};

    for (const char* key : keys) {

        size_t pos = 0;

        while ((pos = out.find(key, pos)) != std::string::npos) {

            pos += std::strlen(key);

            size_t end = out.find('&', pos);

            if (end == std::string::npos)

                end = out.size();

            out.replace(pos, end - pos, "***");

        }

    }

    return out;

}



std::string snippet(const std::string& text, size_t maxLen = 240)

{

    if (text.size() <= maxLen)

        return text;

    return text.substr(0, maxLen) + "…";

}



void configureCurl(CURL* curl)

{

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NXStation/1.0");

#ifdef __SWITCH__

    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

#endif

}



/** Byte size of a media entry, or SIZE_MAX when the API omits it. */
size_t mediaBytes(const Json& media)

{

    for (const char* key : {"taille", "size"}) {

        if (media.contains(key) && media[key].is_number()) {

            const size_t bytes = media[key].get<size_t>();

            if (bytes > 0)

                return bytes;

        }

    }

    return SIZE_MAX;

}



/** Ask ScreenScraper to resize/transcode server-side so the download itself is small. */
std::string withServerResize(const std::string& url, int maxWidth)

{

    if (url.find("screenscraper.fr") == std::string::npos)

        return url;

    std::string out = url;

    out += (out.find('?') == std::string::npos) ? '?' : '&';

    out += "maxwidth=" + std::to_string(maxWidth) + "&outputformat=jpg";

    return out;

}



std::string preferJpegUrl(const std::string& url)

{

    const auto dot = url.rfind('.');

    if (dot == std::string::npos)

        return url;

    std::string ext = url.substr(dot);

    std::transform(ext.begin(), ext.end(), ext.begin(),

                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".png" || ext == ".webp")

        return url.substr(0, dot) + ".jpg";

    return url;

}



bool gameNeedsArtDownload(const GameItem& game, const SystemConfig& system)

{

    GameMetadata probe = game.meta;

    AppState::instance().metaCache().applyLocalMedia(probe, system.id, FileSystem::stemOf(game.path));



    const auto hasFile = [](const std::string& path) {

        return !path.empty() && FileSystem::exists(path);

    };



    auto& cfg = Config::instance();

    if (cfg.scrapeBoxArt() && !hasFile(probe.boxArtPath))

        return true;

    if (cfg.scrapeThumbnail() && !hasFile(probe.logoPath))

        return true;

    return false;

}



void logScraperConfig(const SystemConfig& system)

{

    auto& cfg = Config::instance();

    const int ssId = system.ssSystemId;

    SF_LOG_I("Scraper", "Config: devid=%s softname=%s user=%s system=%s ssSystemId=%d",

             screenscraper::kDevId,

             cfg.screenscraperSoftName().c_str(),

             cfg.screenscraperUser().empty() ? "(none)" : "set",

             system.id.c_str(), ssId);

    if (cfg.screenscraperUser().empty())

        SF_LOG_W("Scraper", "ssid (website login) is empty — set credentials in Settings or %s",
                 paths::USER_SCREENSCRAPER_PATH);

    if (ssId == 0)

        SF_LOG_W("Scraper", "ssSystemId is 0 for '%s' — set it in %s (see ScreenScraper system list)",
                 system.id.c_str(), paths::CONFIG_PATH);

}



} // namespace



ScraperService::ScraperService() = default;

ScraperService::~ScraperService() = default;



void ScraperService::cancelInflight()

{

    {

        std::lock_guard<std::mutex> lock(inFlightMutex_);

        inFlightPaths_.clear();

    }

    {

        std::lock_guard<std::mutex> lock(slotMutex_);

        slotBusy_ = false;

    }

    slotCv_.notify_all();

}



void ScraperService::releaseInFlight(const std::string& path)

{

    std::lock_guard<std::mutex> lock(inFlightMutex_);

    inFlightPaths_.erase(path);

}



bool ScraperService::acquireSlot()

{

    std::unique_lock<std::mutex> lock(slotMutex_);

    slotCv_.wait(lock, [this] {

        return !slotBusy_ || AppState::instance().isHandoffPending();

    });

    if (AppState::instance().isHandoffPending())

        return false;

    slotBusy_ = true;

    return true;

}



void ScraperService::releaseSlot()

{

    std::lock_guard<std::mutex> lock(slotMutex_);

    slotBusy_ = false;

    slotCv_.notify_one();

}



void ScraperService::scrapeAsync(const GameItem& game, const SystemConfig& system, ScrapeCallback cb)
{
    const std::string credErr = scrapeCredentialError();
    if (!credErr.empty()) {
        SF_LOG_W("Scraper", "%s", credErr.c_str());
        if (cb) {
            brls::sync([cb = std::move(cb), credErr]() mutable {
                cb(false, {}, credErr);
            });
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(inFlightMutex_);

        if (inFlightPaths_.count(game.path))

            return;

        inFlightPaths_.insert(game.path);

    }



    AppState::instance().pool().enqueue([this, game, system, cb = std::move(cb)]() mutable {

        const std::string romPath = game.path;



        if (AppState::instance().isHandoffPending()) {

            releaseInFlight(romPath);

            return;

        }



        if (rateLimitMs_ > 0)

            std::this_thread::sleep_for(std::chrono::milliseconds(rateLimitMs_));



        if (AppState::instance().isHandoffPending()) {

            releaseInFlight(romPath);

            return;

        }



        if (!acquireSlot()) {

            releaseInFlight(romPath);

            return;

        }



        std::string error;

        GameMetadata meta;

        try {

            meta = scrapeBlocking(game, system, ScrapeMode::Full, error);

        } catch (...) {

            releaseSlot();

            releaseInFlight(romPath);

            return;

        }

        const bool ok = error.empty() && meta.scraped;



        releaseSlot();

        releaseInFlight(romPath);



        brls::sync([cb = std::move(cb), ok, meta = std::move(meta), error = std::move(error)]() mutable {

            if (AppState::instance().isHandoffPending())

                return;

            cb(ok, std::move(meta), std::move(error));

        });

    });

}



std::string ScraperService::buildQueryUrl(const GameItem& game, const SystemConfig& system,

                                          const std::string& crc)

{

    auto& cfg = Config::instance();

    std::ostringstream url;

    url << "https://www.screenscraper.fr/api2/jeuInfos.php"

        << "?devid=" << urlEncode(screenscraper::kDevId)

        << "&devpassword=" << urlEncode(screenscraper::kDevPassword)

        << "&softname=" << urlEncode(cfg.screenscraperSoftName())

        << "&output=json"

        << "&romtype=rom"

        << "&systemeid=" << system.ssSystemId

        << "&romnom=" << urlEncode(game.filename);



    if (!cfg.screenscraperUser().empty()) {
        url << "&ssid=" << urlEncode(cfg.screenscraperUser())
            << "&sspassword=" << urlEncode(cfg.screenscraperPassword());
    } else {
        SF_LOG_E("Scraper", "buildQueryUrl without website login — should be blocked earlier");
    }

    if (!crc.empty())

        url << "&crc=" << crc;



    return url.str();

}



bool ScraperService::downloadAsset(const std::string& url, const std::string& dest,
                                   bool preserveFormat)

{

    if (url.empty())

        return false;



    auto slash = dest.find_last_of('/');

    if (slash != std::string::npos)

        FileSystem::createDirectories(dest.substr(0, slash));



    const std::string jpegUrl = preserveFormat ? url : preferJpegUrl(url);

    const auto tryOnce = [&](const std::string& fetchUrl) -> bool {

        CURL* curl = curl_easy_init();

        if (!curl)

            return false;



        std::ofstream out(dest, std::ios::binary | std::ios::trunc);

        if (!out) {

            curl_easy_cleanup(curl);

            SF_LOG_W("Scraper", "Asset write open failed: %s", dest.c_str());

            return false;

        }



        curl_easy_setopt(curl, CURLOPT_URL, fetchUrl.c_str());

        configureCurl(curl);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* p, size_t s, size_t n, void* u) -> size_t {

            auto* f = static_cast<std::ofstream*>(u);

            f->write(p, static_cast<std::streamsize>(s * n));

            return s * n;

        });

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);



        CURLcode code = curl_easy_perform(curl);

        long status = 0;

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        curl_easy_cleanup(curl);

        out.close();



        if (code != CURLE_OK || status < 200 || status >= 300) {

            FileSystem::removeFile(dest);

            SF_LOG_W("Scraper", "Asset download failed HTTP %ld curl=%s url=%s",

                     status, curl_easy_strerror(code), maskUrl(fetchUrl).c_str());

            return false;

        }

        return true;

    };



    if (tryOnce(jpegUrl))

        return true;

    if (jpegUrl != url)

        return tryOnce(url);

    return false;

}



bool ScraperService::parseScreenScraperJson(const std::string& json, GameMetadata& out,
                                            std::string& error,
                                            std::vector<std::string>* manualUrlsOut)

{

    try {

        auto root = Json::parse(json);

        if (!root.contains("response")) {

            error = "Malformed ScreenScraper response (no response node)";

            return false;

        }



        const auto& response = root["response"];

        if (response.contains("erreur")) {

            const auto& err = response["erreur"];

            const std::string api = err.value("api", std::string());

            const std::string msg = err.value("message", api);

            error = msg.empty() ? "ScreenScraper API error" : msg;

            SF_LOG_W("Scraper", "API error: %s", error.c_str());

            return false;

        }



        if (!response.contains("jeu")) {

            error = "No match — ScreenScraper could not identify this ROM";

            SF_LOG_W("Scraper", "No jeu match in response: %s", snippet(json).c_str());

            return false;

        }

        const auto& jeu = response["jeu"];

        auto fixText = [](std::string s) { return Utf8::ensureUtf8(std::move(s)); };

        if (jeu.contains("noms") && jeu["noms"].is_array() && jeu["noms"].size() > 0) {

            out.title = fixText(jeu["noms"][0].value("text", out.title));

        } else if (jeu.contains("nom")) {

            out.title = fixText(jeu.value("nom", out.title));

        }



        if (jeu.contains("synopsis") && jeu["synopsis"].is_array() && jeu["synopsis"].size() > 0)

            out.description = fixText(jeu["synopsis"][0].value("text", std::string()));



        if (jeu.contains("developpeur"))

            out.developer = fixText(jeu["developpeur"].value("text", std::string()));

        if (jeu.contains("editeur"))

            out.publisher = fixText(jeu["editeur"].value("text", std::string()));

        if (jeu.contains("dates") && jeu["dates"].is_array() && jeu["dates"].size() > 0)

            out.releaseDate = fixText(jeu["dates"][0].value("text", std::string()));

        if (jeu.contains("genres") && jeu["genres"].is_array() && jeu["genres"].size() > 0) {

            if (jeu["genres"][0].contains("noms") && jeu["genres"][0]["noms"].is_array()

                && jeu["genres"][0]["noms"].size() > 0)

                out.genre = fixText(jeu["genres"][0]["noms"][0].value("text", std::string()));

        }



        const bool optimized = Config::instance().scrapeOptimizedMedia();

        auto pickMediaTypes = [&](std::initializer_list<const char*> types) -> std::string {

            if (!jeu.contains("medias") || !jeu["medias"].is_array())

                return {};

            for (const char* want : types) {

                std::string first;

                std::string smallest;

                size_t smallestSize = SIZE_MAX;

                for (const auto& m : jeu["medias"]) {

                    if (m.value("type", std::string()) != want)

                        continue;

                    const std::string url = m.value("url", std::string());

                    if (url.empty())

                        continue;

                    if (first.empty())

                        first = url;

                    if (!optimized)

                        return url;

                    const size_t bytes = mediaBytes(m);

                    if (bytes != SIZE_MAX && bytes < smallestSize) {

                        smallestSize = bytes;

                        smallest = url;

                    }

                }

                // Most media entries carry no size field, so fall back to the first match.

                if (!smallest.empty())

                    return smallest;

                if (!first.empty())

                    return first;

            }

            return {};

        };



        out.boxArtPath = pickMediaTypes({"box-2D", "box-2D-moved"});

        out.logoPath = pickMediaTypes({"ss", "screen", "screenmarquee"});

        auto pickVideo = [&]() -> std::string {

            if (!jeu.contains("medias") || !jeu["medias"].is_array())

                return {};

            if (!optimized) {

                for (const auto& m : jeu["medias"]) {

                    if (m.value("type", std::string()) == "video")

                        return m.value("url", std::string());

                }

                for (const auto& m : jeu["medias"]) {

                    if (m.value("type", std::string()) == "video-normalized")

                        return m.value("url", std::string());

                }

                return {};

            }

            std::string normalized;

            std::string plain;

            std::string smallest;

            size_t smallestSize = SIZE_MAX;

            for (const auto& m : jeu["medias"]) {

                const std::string type = m.value("type", std::string());

                if (type != "video" && type != "video-normalized")

                    continue;

                const std::string mediaUrl = m.value("url", std::string());

                if (mediaUrl.empty())

                    continue;

                if (type == "video-normalized" && normalized.empty())

                    normalized = mediaUrl;

                if (type == "video" && plain.empty())

                    plain = mediaUrl;

                const size_t bytes = mediaBytes(m);

                if (bytes != SIZE_MAX && bytes < smallestSize) {

                    smallestSize = bytes;

                    smallest = mediaUrl;

                }

            }

            if (!smallest.empty())

                return smallest;

            // "video-normalized" is the re-encoded, smaller variant when sizes are unknown.

            if (!normalized.empty())

                return normalized;

            return plain;

        };

        out.videoPath = pickVideo();

        if (manualUrlsOut) {
            manualUrlsOut->clear();
            if (jeu.contains("medias") && jeu["medias"].is_array()) {
                for (const auto& m : jeu["medias"]) {
                    if (m.value("type", std::string()) != "manuel")
                        continue;
                    const std::string url = m.value("url", std::string());
                    if (!url.empty())
                        manualUrlsOut->push_back(url);
                }
            }
        }

        out.scraped = true;

        return true;

    } catch (const std::exception& ex) {

        error = std::string("JSON error: ") + ex.what();

        return false;

    }

}



GameMetadata ScraperService::scrapeBlocking(const GameItem& game, const SystemConfig& system,

                                            ScrapeMode mode, std::string& error,

                                            ScrapeProgressCb stepProgress)

{

    GameMetadata meta = game.meta;

    meta.romPath = game.path;

    meta.systemId = system.id;

    if (meta.title.empty())

        meta.title = game.displayName;



    (void)mode;



    SF_LOG_I("Scraper", "Start %s [%s]", game.filename.c_str(), system.id.c_str());

    logScraperConfig(system);

    const std::string credErr = scrapeCredentialError();
    if (!credErr.empty()) {
        error = credErr;
        SF_LOG_W("Scraper", "%s", error.c_str());
        return meta;
    }

    if (system.ssSystemId <= 0) {
        error = "ssSystemId missing in roms_config.json for " + system.id;
        SF_LOG_W("Scraper", "%s", error.c_str());
        return meta;
    }

    meta.crc32 = Hash::crc32File(game.path);

    if (meta.crc32.empty())

        SF_LOG_W("Scraper", "CRC32 empty for %s", game.path.c_str());

    else

        SF_LOG_I("Scraper", "CRC32=%s", meta.crc32.c_str());



    const std::string url = buildQueryUrl(game, system, meta.crc32);

    SF_LOG_I("Scraper", "GET %s", maskUrl(url).c_str());



    std::string body;

    CURL* curl = curl_easy_init();

    if (!curl) {

        error = "curl init failed";

        SF_LOG_E("Scraper", "%s", error.c_str());

        return meta;

    }



    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    configureCurl(curl);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* p, size_t s, size_t n, void* u) -> size_t {

        static_cast<std::string*>(u)->append(p, s * n);

        return s * n;

    });

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);



    CURLcode code = curl_easy_perform(curl);

    long status = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(curl);



    SF_LOG_I("Scraper", "HTTP %ld curl=%d bodyBytes=%zu", status, static_cast<int>(code), body.size());



    if (code != CURLE_OK) {

        error = curl_easy_strerror(code);

        SF_LOG_W("Scraper", "Network error for %s: %s", game.filename.c_str(), error.c_str());

        return meta;

    }

    if (status == 429) {
        error = "Rate limited (HTTP 429) — wait a moment and try again";
        SF_LOG_W("Scraper", "Rate limited for %s", game.filename.c_str());
        return meta;
    }

    if (status == 404) {
        error = "No match (HTTP 404) — ScreenScraper has no entry for this ROM";
        SF_LOG_W("Scraper", "HTTP 404 for %s body=%s", game.filename.c_str(), snippet(body).c_str());
        return meta;
    }

    if (status == 401 || status == 403) {
        error = "Server rejected (HTTP " + std::to_string(status)
                + ") — check ScreenScraper login or daily quota";
        SF_LOG_W("Scraper", "HTTP %ld for %s body=%s", status, game.filename.c_str(),
                 snippet(body).c_str());
        return meta;
    }

    if (status == 423) {
        error = "ROM locked (HTTP 423) — file may be too large for your ScreenScraper tier";
        SF_LOG_W("Scraper", "HTTP 423 for %s", game.filename.c_str());
        return meta;
    }

    if (status < 200 || status >= 300) {
        error = "Request failed (HTTP " + std::to_string(status) + ")";
        SF_LOG_W("Scraper", "HTTP error for %s: %s body=%s", game.filename.c_str(), error.c_str(),
                 snippet(body).c_str());
        return meta;
    }



    if (body.empty()) {

        error = "Empty response body";

        SF_LOG_W("Scraper", "Empty body for %s", game.filename.c_str());

        return meta;

    }



    std::string boxUrl, logoUrl, videoUrl;
    std::vector<std::string> manualUrls;

    if (!parseScreenScraperJson(body, meta, error, &manualUrls)) {

        SF_LOG_W("Scraper", "Parse failed for %s: %s", game.filename.c_str(), error.c_str());

        return meta;

    }



    boxUrl = meta.boxArtPath;

    logoUrl = meta.logoPath;

    videoUrl = meta.videoPath;



    auto& cache = AppState::instance().metaCache();

    const std::string stem = FileSystem::stemOf(game.path);



    meta.boxArtPath.clear();

    meta.logoPath.clear();

    meta.videoPath.clear();



    auto reportAsset = [&](const char* label, const std::string& detail, bool hasResult, bool success) {
        if (!stepProgress)
            return;
        ScrapeProgress step;
        step.gameName = game.displayName;
        step.phase = label;
        step.detail = detail;
        step.hasResult = hasResult;
        step.success = success;
        stepProgress(step);
    };



    auto& cfg = Config::instance();



    if (cfg.scrapeBoxArt() && !boxUrl.empty()) {

        reportAsset("Box art", "Downloading…", false, false);

        std::string dest = cache.artworkPath(system.id, stem, "box");

        const bool ok = downloadAsset(

            cfg.scrapeOptimizedMedia() ? withServerResize(boxUrl, 512) : boxUrl, dest);

        if (ok) {

            if (cfg.scrapeOptimizedMedia())

                optimizeScrapedImage(dest, false);

            meta.boxArtPath = dest;

        }

        reportAsset("Box art", ok ? "OK" : "Failed", true, ok);

    } else if (cfg.scrapeBoxArt()) {

        reportAsset("Box art", "Not available", true, false);

    } else {

        reportAsset("Box art", "Skipped", true, false);

    }

    if (cfg.scrapeThumbnail() && !logoUrl.empty()) {

        reportAsset("Thumbnail", "Downloading…", false, false);

        std::string dest = cache.artworkPath(system.id, stem, "thumb");

        const bool ok = downloadAsset(

            cfg.scrapeOptimizedMedia() ? withServerResize(logoUrl, 320) : logoUrl, dest);

        if (ok) {

            if (cfg.scrapeOptimizedMedia())

                optimizeScrapedImage(dest, true);

            meta.logoPath = dest;

        }

        reportAsset("Thumbnail", ok ? "OK" : "Failed", true, ok);

    } else if (cfg.scrapeThumbnail()) {

        reportAsset("Thumbnail", "Not available", true, false);

    } else {

        reportAsset("Thumbnail", "Skipped", true, false);

    }

    if (cfg.scrapeVideo() && !videoUrl.empty() && AppState::instance().videoAllowed()) {

        reportAsset("Video", "Downloading…", false, false);

        std::string dest = cache.videoCachePath(system.id, stem);

        const bool ok = downloadAsset(videoUrl, dest);

        if (ok)

            meta.videoPath = dest;

        reportAsset("Video", ok ? "OK" : "Failed", true, ok);

    } else if (!cfg.scrapeVideo()) {

        reportAsset("Video", "Skipped", true, false);

    } else if (!videoUrl.empty()) {

        reportAsset("Video", "Skipped (Applet Mode)", true, false);

    } else {

        reportAsset("Video", "Not available", true, false);

    }

    if (cfg.scrapeManual() && !manualUrls.empty()) {
        reportAsset("Manual", "Downloading…", false, false);

        const std::string romRoot = system.path;
        const std::string pdfDest = ManualPages::pdfPath(romRoot, stem);
        FileSystem::createDirectories(FileSystem::join(romRoot, "manuals"));

        bool manualOk = false;
        for (const std::string& manualUrl : manualUrls) {
            if (!ManualPages::urlLooksLikePdf(manualUrl))
                continue;
            if (downloadAsset(manualUrl, pdfDest, true)) {
                meta.manualPath = pdfDest;
                manualOk = true;
                break;
            }
        }

        reportAsset("Manual", manualOk ? "OK" : "Failed", true, manualOk);
    } else if (!cfg.scrapeManual()) {
        reportAsset("Manual", "Skipped", true, false);
    } else {
        reportAsset("Manual", "Not available", true, false);
    }

    cache.save(meta);

    SF_LOG_I("Scraper", "Scraped OK: %s", meta.title.c_str());

    return meta;

}



void ScraperService::requestAbort()

{

    abortRequested_ = true;

    SF_LOG_I("Scraper", "Batch abort requested");

}



void ScraperService::clearAbort()

{

    abortRequested_ = false;

}



void ScraperService::runBatchAsync(std::vector<GameItem> games, SystemConfig system, ScrapeMode mode,

                                   ScrapeProgressCb onProgress, ScrapeBatchDoneCb onDone)
{
    const std::string credErr = scrapeCredentialError();
    if (!credErr.empty()) {
        SF_LOG_W("Scraper", "%s", credErr.c_str());
        if (onDone) {
            brls::sync([onDone = std::move(onDone), credErr]() mutable {
                brls::Application::notify(credErr);
                onDone(0, 0, 0, false);
            });
        }
        return;
    }

    clearAbort();

    cancelInflight();



    AppState::instance().pool().enqueue(

        [this, games = std::move(games), system = std::move(system), mode,

         onProgress = std::move(onProgress), onDone = std::move(onDone)]() mutable {

            struct NetGuard {
                ~NetGuard() { Network::releaseSession(); }
            } netGuard;

            size_t succeeded = 0;

            size_t failed = 0;

            size_t skipped = 0;



            SF_LOG_I("Scraper", "Batch start: %zu games mode=%d", games.size(),
                     static_cast<int>(mode));

            ScrapeLog::open();
            ScrapeLog::writef("=== Batch start system=%s mode=%d games=%zu ===", system.id.c_str(),
                              static_cast<int>(mode), games.size());

            if (!Network::waitForConnection(30)) {

                SF_LOG_W("Scraper", "Network not available for batch scrape");

            }

            logScraperConfig(system);



            for (size_t i = 0; i < games.size(); ++i) {

                if (abortRequested_ || AppState::instance().isHandoffPending()) {

                    const size_t ok = succeeded, fail = failed, skip = skipped;

                    brls::sync([onDone, ok, fail, skip]() { onDone(ok, fail, skip, true); });

                    return;

                }



                const GameItem& game = games[i];

                ScrapeProgress progress;

                progress.index = i + 1;

                progress.total = games.size();

                progress.succeeded = succeeded;

                progress.failed = failed;

                progress.skipped = skipped;

                progress.gameName = game.displayName;



                if (mode == ScrapeMode::MissingArtOnly && !gameNeedsArtDownload(game, system)) {

                    ++skipped;

                    progress.skipped = skipped;

                    progress.phase = "Skipped";

                    progress.detail = "Art already on SD";

                    ScrapeLog::writef("[%zu/%zu] %s — Skipped: %s", progress.index, progress.total,
                                      progress.gameName.c_str(), progress.detail.c_str());

                    brls::sync([progress, onProgress]() { onProgress(progress); });

                    continue;

                }



                progress.phase = "Scraping";

                progress.detail = "Hashing ROM & querying ScreenScraper…";

                brls::sync([progress, onProgress]() { onProgress(progress); });



                if (rateLimitMs_ > 0)

                    std::this_thread::sleep_for(std::chrono::milliseconds(rateLimitMs_));



                if (!acquireSlot()) {

                    ++failed;

                    progress.failed = failed;

                    progress.phase = "Failed";

                    progress.detail = "Scraper busy";

                    brls::sync([progress, onProgress]() { onProgress(progress); });

                    continue;

                }



                std::string error;

                GameMetadata meta;

                auto stepProgress = [&](const ScrapeProgress& step) {
                    if (step.hasResult && !step.detail.empty() && step.detail != "Downloading…") {
                        ScrapeLog::writef("  %s — %s: %s", game.displayName.c_str(),
                                          step.phase.c_str(), step.detail.c_str());
                    }
                    ScrapeProgress p = step;
                    p.index = i + 1;
                    p.total = games.size();
                    p.succeeded = succeeded;
                    p.failed = failed;
                    p.skipped = skipped;
                    if (p.gameName.empty())
                        p.gameName = game.displayName;
                    brls::sync([p, onProgress]() { onProgress(p); });
                };

                try {

                    meta = scrapeBlocking(game, system, mode, error, stepProgress);

                } catch (...) {

                    releaseSlot();

                    ++failed;

                    progress.failed = failed;

                    progress.phase = "Failed";

                    progress.detail = "Unexpected error";

                    brls::sync([progress, onProgress]() { onProgress(progress); });

                    continue;

                }



                releaseSlot();



                if (!error.empty() || !meta.scraped) {

                    ++failed;

                    progress.failed = failed;

                    progress.phase = "Failed";

                    progress.detail = error.empty() ? "No metadata returned" : error;

                    ScrapeLog::writef("[%zu/%zu] %s — Failed: %s", progress.index, progress.total,
                                      progress.gameName.c_str(), progress.detail.c_str());

                } else {

                    ++succeeded;

                    progress.succeeded = succeeded;

                    progress.phase = "OK";
                    progress.detail.clear();
                    ScrapeLog::writef("[%zu/%zu] %s — OK", progress.index, progress.total,
                                      progress.gameName.c_str());

                    AppState::instance().updateGameMetadata(system.id, game.path, meta);

                }



                brls::sync([progress, onProgress]() { onProgress(progress); });

            }



            const size_t ok = succeeded, fail = failed, skip = skipped;

            const bool aborted = abortRequested_.load();

            ScrapeLog::writef("=== Batch done: OK %zu Fail %zu Skip %zu%s ===", ok, fail, skip,
                              aborted ? " (aborted)" : "");

            brls::sync([onDone, ok, fail, skip, aborted]() { onDone(ok, fail, skip, aborted); });

        });

}



} // namespace sf

