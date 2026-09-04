#include "app/Config.hpp"
#include "app/ScreenScraperCredentials.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiTransition.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace sf {

namespace {

bool isPlaceholderCredential(const std::string& s)
{
    return s.empty() || s == "xxx" || s == "XXX";
}

/** Systems removed from bundled defaults (no usable Switch RetroArch core). */
const std::unordered_set<std::string>& retiredSystemIds()
{
    // ps2 was dropped earlier; 3DS is bundled again (tico-azahar / similar).
    static const std::unordered_set<std::string> kRetired{"ps2"};
    return kRetired;
}

/** Drop retired systems from memory and rewrite SD roms_config / user_cores if needed. */
void pruneRetiredSystems(std::vector<SystemConfig>& systems, const std::string& configPath,
                         std::unordered_map<std::string, std::string>& userCores)
{
    const auto& retired = retiredSystemIds();
    const size_t before = systems.size();
    systems.erase(std::remove_if(systems.begin(), systems.end(),
                                 [&](const SystemConfig& s) { return retired.count(s.id) != 0; }),
                  systems.end());
    const bool systemsChanged = systems.size() != before;

    bool coresChanged = false;
    for (const auto& id : retired) {
        if (userCores.erase(id) > 0)
            coresChanged = true;
    }

    if (systemsChanged && configPath == paths::CONFIG_PATH && FileSystem::exists(configPath)) {
        try {
            auto j = Json::parse(FileSystem::readFile(configPath));
            if (j.contains("systems") && j["systems"].is_array()) {
                Json kept = Json::array();
                for (const auto& s : j["systems"]) {
                    const std::string id = s.value("id", std::string());
                    if (retired.count(id) == 0)
                        kept.push_back(s);
                }
                j["systems"] = std::move(kept);
                if (FileSystem::writeFile(configPath, j.dump(2)))
                    SF_LOG_I("Config", "Removed retired systems from %s", configPath.c_str());
            }
        } catch (const std::exception& ex) {
            SF_LOG_W("Config", "Failed to prune roms_config.json: %s", ex.what());
        }
    }

    if (coresChanged) {
        Json cores = Json::object();
        for (const auto& [id, path] : userCores)
            cores[id] = path;
        Json root = Json::object();
        root["cores"] = cores;
        if (FileSystem::writeFile(paths::USER_CORES_PATH, root.dump(2)))
            SF_LOG_I("Config", "Removed retired systems from user_cores.json");
    }

    if (systemsChanged)
        SF_LOG_I("Config", "Pruned %zu retired system(s)", before - systems.size());
}

} // namespace

Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

std::string Config::screenscraperSoftName() const
{
    return screenscraper::kSoftName;
}

bool Config::hasScreenScraperWebsiteLogin() const
{
    return !isPlaceholderCredential(ssUser_) && !isPlaceholderCredential(ssPassword_);
}

bool Config::seedReferenceSettingsFiles()
{
    if (!FileSystem::exists(paths::CONFIG_PATH)) {
        const std::string bundled = FileSystem::readFile(paths::CONFIG_FALLBACK);
        if (!bundled.empty()) {
            FileSystem::writeFile(paths::CONFIG_PATH, bundled);
            SF_LOG_I("Config", "Created reference %s from bundled defaults", paths::CONFIG_PATH);
        }
    }

    if (!FileSystem::exists(paths::USER_CORES_PATH)) {
        const std::string configPath =
            FileSystem::exists(paths::CONFIG_PATH) ? paths::CONFIG_PATH : paths::CONFIG_FALLBACK;
        const std::string data = FileSystem::readFile(configPath);
        if (!data.empty()) {
            try {
                const auto j = Json::parse(data);
                Json cores = Json::object();
                if (j.contains("systems") && j["systems"].is_array()) {
                    for (const auto& s : j["systems"]) {
                        const std::string id = s.value("id", std::string());
                        const std::string core = s.value("core", std::string());
                        if (!id.empty() && !core.empty())
                            cores[id] = core;
                    }
                }
                Json root = Json::object();
                root["cores"] = cores;
                if (FileSystem::writeFile(paths::USER_CORES_PATH, root.dump(2))) {
                    SF_LOG_I("Config", "Created reference %s from system defaults", paths::USER_CORES_PATH);
                }
            } catch (const std::exception& ex) {
                SF_LOG_W("Config", "Failed to seed user_cores.json: %s", ex.what());
            }
        }
    }

    if (!FileSystem::exists(paths::SYSTEM_DESCRIPTIONS_PATH)) {
        const std::string bundled = FileSystem::readFile(paths::SYSTEM_DESCRIPTIONS_FALLBACK);
        if (!bundled.empty()) {
            FileSystem::writeFile(paths::SYSTEM_DESCRIPTIONS_PATH, bundled);
            SF_LOG_I("Config", "Created reference %s from bundled defaults",
                     paths::SYSTEM_DESCRIPTIONS_PATH);
        }
    }

    return true;
}

bool Config::load(const std::string& path)
{
    std::string data = FileSystem::readFile(path);
    if (data.empty() && path != paths::CONFIG_FALLBACK) {
        SF_LOG_W("Config", "Missing %s, trying romfs fallback", path.c_str());
        data = FileSystem::readFile(paths::CONFIG_FALLBACK);
        configPath_ = paths::CONFIG_FALLBACK;
    } else {
        configPath_ = path;
    }

    if (data.empty()) {
        SF_LOG_E("Config", "No configuration found");
        return false;
    }

    try {
        auto j = Json::parse(data);
        systems_.clear();

        if (j.contains("systems") && j["systems"].is_array()) {
            for (const auto& s : j["systems"]) {
                SystemConfig sc;
                sc.id = s.value("id", std::string());
                sc.name = s.value("name", sc.id);
                sc.path = s.value("path", std::string());
                sc.core = s.value("core", std::string());
                if (s.contains("extensions") && s["extensions"].is_array()) {
                    for (const auto& e : s["extensions"])
                        sc.extensions.push_back(FileSystem::toLower(e.get<std::string>()));
                }
                if (s.contains("ssSystemId") && s["ssSystemId"].is_number())
                    sc.ssSystemId = s["ssSystemId"].get<int>();
                else if (s.contains("screenscraper_system_id") && s["screenscraper_system_id"].is_number())
                    sc.ssSystemId = s["screenscraper_system_id"].get<int>();
                if (!sc.id.empty() && !sc.path.empty())
                    systems_.push_back(std::move(sc));
            }
        }

        if (j.contains("retroarch")) {
            const auto& ra = j["retroarch"];
            retroArchPath_ = ra.value("path", retroArchPath_);
        } else if (j.contains("settings") && j["settings"].contains("retroarch_path")) {
            retroArchPath_ = j["settings"].value("retroarch_path", retroArchPath_);
        }

        if (j.contains("settings")) {
            const auto& st = j["settings"];
            videoEnabled_ = st.value("video_enabled", true);
            hoverDelaySeconds_ = st.value("hover_delay_seconds", 1.0f);
            textureCacheLimit_ = st.value("texture_cache_limit", static_cast<size_t>(48));
            requestDelayMs_ = st.value("request_delay_ms", static_cast<size_t>(350));
        }

        SF_LOG_I("Config", "Loaded %zu systems from %s", systems_.size(), configPath_.c_str());
        mergeBundledSsSystemIds();
        mergeNewBundledSystems();
        loadUserCores();
        pruneRetiredSystems(systems_, configPath_, userCoreOverrides_);
        loadUserScreenScraper();
        loadUserCloud();
        loadUserSettings();
        applyRomsRootOverride();
        hoverDelaySeconds_ = static_cast<float>(videoPreviewDelaySeconds_);
        return !systems_.empty();
    } catch (const std::exception& ex) {
        SF_LOG_E("Config", "JSON parse error: %s", ex.what());
        return false;
    }
}

bool Config::reload()
{
    return load(configPath_.empty() ? paths::CONFIG_PATH : configPath_);
}

void Config::applyRomsRootOverride()
{
    if (romsRootOverride_.empty())
        return;

    const std::string root = defaultRomsRoot();
    for (auto& sys : systems_) {
        if (sys.path.rfind(root, 0) == 0)
            sys.path = romsRootOverride_ + sys.path.substr(root.size());
    }
}

void Config::setRomsRootOverride(const std::string& v)
{
    std::string next = v;
    while (!next.empty() && (next.back() == '/' || next.back() == '\\'))
        next.pop_back();
    if (next == defaultRomsRoot())
        next.clear();
    romsRootOverride_ = next;
}

const SystemConfig* Config::findSystem(const std::string& id) const
{
    for (const auto& s : systems_) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

std::string Config::coreFor(const std::string& systemId) const
{
    auto it = userCoreOverrides_.find(systemId);
    if (it != userCoreOverrides_.end() && !it->second.empty())
        return it->second;

    if (const SystemConfig* sys = findSystem(systemId))
        return sys->core;
    return {};
}

void Config::setSystemCore(const std::string& systemId, const std::string& corePath)
{
    if (corePath.empty())
        userCoreOverrides_.erase(systemId);
    else
        userCoreOverrides_[systemId] = corePath;
}

void Config::mergeBundledSsSystemIds()
{
    try {
        const std::string romfs = FileSystem::readFile(paths::CONFIG_FALLBACK);
        if (romfs.empty())
            return;

        const auto j = Json::parse(romfs);
        if (!j.contains("systems") || !j["systems"].is_array())
            return;

        std::unordered_map<std::string, int> bundled;
        for (const auto& s : j["systems"]) {
            const std::string id = s.value("id", std::string());
            if (id.empty())
                continue;
            if (s.contains("ssSystemId") && s["ssSystemId"].is_number())
                bundled[id] = s["ssSystemId"].get<int>();
        }

        for (auto& sys : systems_) {
            if (sys.ssSystemId > 0)
                continue;
            const auto it = bundled.find(sys.id);
            if (it != bundled.end() && it->second > 0)
                sys.ssSystemId = it->second;
        }
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "Bundled ssSystemId merge failed: %s", ex.what());
    }
}

void Config::mergeNewBundledSystems()
{
    // Only upgrade the live SD config — never rewrite romfs or invent systems when
    // the user is already running purely from the bundled fallback.
    if (configPath_ != paths::CONFIG_PATH)
        return;

    try {
        const std::string romfsData = FileSystem::readFile(paths::CONFIG_FALLBACK);
        if (romfsData.empty())
            return;

        const auto bundled = Json::parse(romfsData);
        if (!bundled.contains("systems") || !bundled["systems"].is_array())
            return;

        std::unordered_set<std::string> known;
        known.reserve(systems_.size());
        for (const auto& s : systems_)
            known.insert(s.id);

        std::vector<Json> toAppend;
        for (const auto& s : bundled["systems"]) {
            const std::string id = s.value("id", std::string());
            const std::string path = s.value("path", std::string());
            if (id.empty() || path.empty() || known.count(id) != 0)
                continue;

            SystemConfig sc;
            sc.id = id;
            sc.name = s.value("name", id);
            sc.path = path;
            sc.core = s.value("core", std::string());
            if (s.contains("extensions") && s["extensions"].is_array()) {
                for (const auto& e : s["extensions"])
                    sc.extensions.push_back(FileSystem::toLower(e.get<std::string>()));
            }
            if (s.contains("ssSystemId") && s["ssSystemId"].is_number())
                sc.ssSystemId = s["ssSystemId"].get<int>();

            systems_.push_back(std::move(sc));
            known.insert(id);
            toAppend.push_back(s);
        }

        if (toAppend.empty())
            return;

        auto j = Json::parse(FileSystem::readFile(paths::CONFIG_PATH));
        if (!j.contains("systems") || !j["systems"].is_array())
            j["systems"] = Json::array();
        for (const auto& s : toAppend)
            j["systems"].push_back(s);

        if (FileSystem::writeFile(paths::CONFIG_PATH, j.dump(2))) {
            SF_LOG_I("Config", "Merged %zu new bundled system(s) into %s", toAppend.size(),
                     paths::CONFIG_PATH);
        }

        // Seed default cores for newly added systems when user_cores already exists.
        if (FileSystem::exists(paths::USER_CORES_PATH)) {
            try {
                auto coresRoot = Json::parse(FileSystem::readFile(paths::USER_CORES_PATH));
                if (!coresRoot.contains("cores") || !coresRoot["cores"].is_object())
                    coresRoot["cores"] = Json::object();
                bool coresChanged = false;
                for (const auto& s : toAppend) {
                    const std::string id = s.value("id", std::string());
                    const std::string core = s.value("core", std::string());
                    if (id.empty() || core.empty())
                        continue;
                    if (!coresRoot["cores"].contains(id)) {
                        coresRoot["cores"][id] = core;
                        coresChanged = true;
                    }
                }
                if (coresChanged)
                    FileSystem::writeFile(paths::USER_CORES_PATH, coresRoot.dump(2));
            } catch (const std::exception& ex) {
                SF_LOG_W("Config", "Failed to seed cores for merged systems: %s", ex.what());
            }
        }
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "Bundled system merge failed: %s", ex.what());
    }
}

bool Config::loadUserCores()
{
    userCoreOverrides_.clear();
    userCoresPath_ = paths::USER_CORES_PATH;

    std::string data = FileSystem::readFile(userCoresPath_);
    if (data.empty())
        return true;

    try {
        auto j = Json::parse(data);
        if (!j.contains("cores") || !j["cores"].is_object())
            return true;
        for (const auto& [id, val] : j["cores"].items()) {
            if (val.is_string())
                userCoreOverrides_[id] = val.get<std::string>();
        }
        SF_LOG_I("Config", "Loaded %zu core overrides", userCoreOverrides_.size());
        if (userCoreOverrides_.count("megadrive") == 0 && userCoreOverrides_.count("genesis") != 0)
            userCoreOverrides_["megadrive"] = userCoreOverrides_["genesis"];
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "user_cores.json parse error: %s", ex.what());
        return false;
    }
}

bool Config::saveUserCores()
{
    Json cores = Json::object();
    for (const auto& [id, path] : userCoreOverrides_)
        cores[id] = path;

    Json root = Json::object();
    root["cores"] = cores;

    if (!FileSystem::writeFile(paths::USER_CORES_PATH, root.dump(2))) {
        SF_LOG_E("Config", "Failed to save %s", paths::USER_CORES_PATH);
        return false;
    }
    SF_LOG_I("Config", "Saved core overrides to %s", paths::USER_CORES_PATH);
    return true;
}

bool Config::loadUserScreenScraper()
{
    std::string data = FileSystem::readFile(paths::USER_SCREENSCRAPER_PATH);
    if (data.empty())
        return true;

    try {
        auto j = Json::parse(data);
        if (j.contains("ssid") && j["ssid"].is_string()) {
            const std::string v = j["ssid"].get<std::string>();
            if (!isPlaceholderCredential(v))
                ssUser_ = v;
        }
        if (j.contains("sspassword") && j["sspassword"].is_string()) {
            const std::string v = j["sspassword"].get<std::string>();
            if (!isPlaceholderCredential(v))
                ssPassword_ = v;
        }
        SF_LOG_I("Config", "Loaded ScreenScraper overrides from %s", paths::USER_SCREENSCRAPER_PATH);
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "user_screenscraper.json parse error: %s", ex.what());
        return false;
    }
}

bool Config::saveUserScreenScraper()
{
    Json root = Json::object();
    root["ssid"] = ssUser_;
    root["sspassword"] = ssPassword_;

    if (!FileSystem::writeFile(paths::USER_SCREENSCRAPER_PATH, root.dump(2))) {
        SF_LOG_E("Config", "Failed to save %s", paths::USER_SCREENSCRAPER_PATH);
        return false;
    }
    SF_LOG_I("Config", "Saved ScreenScraper credentials to %s", paths::USER_SCREENSCRAPER_PATH);
    return true;
}

bool Config::loadUserSettings()
{
    std::string data = FileSystem::readFile(paths::USER_SETTINGS_PATH);
    if (data.empty())
        return true;

    try {
        auto j = Json::parse(data);
        if (j.contains("nav_sound_enabled") && j["nav_sound_enabled"].is_boolean())
            navSoundEnabled_ = j["nav_sound_enabled"].get<bool>();
        if (j.contains("video_audio_enabled") && j["video_audio_enabled"].is_boolean())
            videoAudioEnabled_ = j["video_audio_enabled"].get<bool>();
        if (j.contains("nav_sound_volume") && j["nav_sound_volume"].is_number())
            navSoundVolume_ = j["nav_sound_volume"].get<float>();
        if (j.contains("video_audio_volume") && j["video_audio_volume"].is_number())
            videoAudioVolume_ = j["video_audio_volume"].get<float>();
        if (j.contains("bgm_enabled") && j["bgm_enabled"].is_boolean())
            bgmEnabled_ = j["bgm_enabled"].get<bool>();
        if (j.contains("bgm_volume") && j["bgm_volume"].is_number())
            bgmVolume_ = j["bgm_volume"].get<float>();
        if (j.contains("hide_empty_systems") && j["hide_empty_systems"].is_boolean())
            hideEmptySystems_ = j["hide_empty_systems"].get<bool>();
        if (j.contains("forwarder_prompt_enabled") && j["forwarder_prompt_enabled"].is_boolean())
            forwarderPromptEnabled_ = j["forwarder_prompt_enabled"].get<bool>();
        if (j.contains("video_enabled") && j["video_enabled"].is_boolean())
            videoEnabled_ = j["video_enabled"].get<bool>();
        if (j.contains("theme") && j["theme"].is_string())
            themeName_ = sf::ui::ThemeManager::migrateThemeName(j["theme"].get<std::string>());
        if (j.contains("game_art_mode") && j["game_art_mode"].is_string()) {
            const std::string mode = j["game_art_mode"].get<std::string>();
            gameArtMode_ = (mode == "thumbnail") ? GameArtMode::Thumbnail : GameArtMode::BoxArt;
        }
        if (j.contains("carousel_transition") && j["carousel_transition"].is_string())
            carouselTransition_ = carouselTransitionFromString(j["carousel_transition"].get<std::string>());
        if (j.contains("system_browser_style") && j["system_browser_style"].is_string())
            systemBrowserStyle_ =
                systemBrowserStyleFromString(j["system_browser_style"].get<std::string>());
        if (j.contains("right_stick_description_scroll") &&
            j["right_stick_description_scroll"].is_boolean())
            rightStickDescriptionScroll_ = j["right_stick_description_scroll"].get<bool>();
        if (j.contains("rom_list_scrollbar") && j["rom_list_scrollbar"].is_boolean())
            romListScrollbar_ = j["rom_list_scrollbar"].get<bool>();
        if (j.contains("video_preview_delay_seconds") && j["video_preview_delay_seconds"].is_number()) {
            videoPreviewDelaySeconds_ =
                static_cast<int>(j["video_preview_delay_seconds"].get<double>());
            if (videoPreviewDelaySeconds_ < 1)
                videoPreviewDelaySeconds_ = 1;
            if (videoPreviewDelaySeconds_ > 6)
                videoPreviewDelaySeconds_ = 6;
        }
        if (j.contains("manual_layout_mode") && j["manual_layout_mode"].is_string())
            manualLayoutMode_ = j["manual_layout_mode"].get<std::string>();
        if (j.contains("scrape_box_art") && j["scrape_box_art"].is_boolean())
            scrapeBoxArt_ = j["scrape_box_art"].get<bool>();
        if (j.contains("scrape_thumbnail") && j["scrape_thumbnail"].is_boolean())
            scrapeThumbnail_ = j["scrape_thumbnail"].get<bool>();
        if (j.contains("scrape_video") && j["scrape_video"].is_boolean())
            scrapeVideo_ = j["scrape_video"].get<bool>();
        if (j.contains("scrape_manual") && j["scrape_manual"].is_boolean())
            scrapeManual_ = j["scrape_manual"].get<bool>();
        if (j.contains("scrape_optimized_media") && j["scrape_optimized_media"].is_boolean())
            scrapeOptimizedMedia_ = j["scrape_optimized_media"].get<bool>();
        if (j.contains("cloud_auto_save_enabled") && j["cloud_auto_save_enabled"].is_boolean())
            cloudAutoSaveEnabled_ = j["cloud_auto_save_enabled"].get<bool>();
        if (j.contains("cloud_provider") && j["cloud_provider"].is_string())
            cloudProvider_ = j["cloud_provider"].get<std::string>();
        if (j.contains("screensaver_idle_seconds") && j["screensaver_idle_seconds"].is_number())
            screensaverIdleSeconds_ = static_cast<int>(j["screensaver_idle_seconds"].get<double>());
        if (j.contains("library_scan_completed") && j["library_scan_completed"].is_boolean())
            libraryScanCompleted_ = j["library_scan_completed"].get<bool>();
        if (j.contains("roms_root_override") && j["roms_root_override"].is_string())
            romsRootOverride_ = j["roms_root_override"].get<std::string>();
        SF_LOG_I("Config", "Loaded user settings from %s", paths::USER_SETTINGS_PATH);
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "user_settings.json parse error: %s", ex.what());
        return false;
    }
}

bool Config::saveUserSettings()
{
    Json root = Json::object();
    root["nav_sound_enabled"] = navSoundEnabled_;
    root["video_audio_enabled"] = videoAudioEnabled_;
    root["nav_sound_volume"] = navSoundVolume_;
    root["video_audio_volume"] = videoAudioVolume_;
    root["bgm_enabled"] = bgmEnabled_;
    root["bgm_volume"] = bgmVolume_;
    root["hide_empty_systems"] = hideEmptySystems_;
    root["forwarder_prompt_enabled"] = forwarderPromptEnabled_;
    root["video_enabled"] = videoEnabled_;
    root["theme"] = themeName_;
    root["game_art_mode"] = gameArtMode_ == GameArtMode::Thumbnail ? "thumbnail" : "box_art";
    root["carousel_transition"] = carouselTransitionToString(carouselTransition_);
    root["system_browser_style"] = systemBrowserStyleToString(systemBrowserStyle_);
    root["right_stick_description_scroll"] = rightStickDescriptionScroll_;
    root["rom_list_scrollbar"] = romListScrollbar_;
    root["video_preview_delay_seconds"] = videoPreviewDelaySeconds_;
    root["manual_layout_mode"] = manualLayoutMode_;
    root["scrape_box_art"] = scrapeBoxArt_;
    root["scrape_thumbnail"] = scrapeThumbnail_;
    root["scrape_video"] = scrapeVideo_;
    root["scrape_manual"] = scrapeManual_;
    root["scrape_optimized_media"] = scrapeOptimizedMedia_;
    root["cloud_auto_save_enabled"] = cloudAutoSaveEnabled_;
    root["cloud_provider"] = cloudProvider_;
    root["screensaver_idle_seconds"] = screensaverIdleSeconds_;
    root["library_scan_completed"] = libraryScanCompleted_;
    root["roms_root_override"] = romsRootOverride_;

    if (!FileSystem::writeFile(paths::USER_SETTINGS_PATH, root.dump(2))) {
        SF_LOG_E("Config", "Failed to save %s", paths::USER_SETTINGS_PATH);
        return false;
    }
    SF_LOG_I("Config", "Saved user settings to %s", paths::USER_SETTINGS_PATH);
    return true;
}

bool Config::loadUserCloud()
{
    std::string data = FileSystem::readFile(paths::USER_CLOUD_PATH);
    if (data.empty())
        return true;

    try {
        auto j = Json::parse(data);
        if (j.contains("last_sync_iso") && j["last_sync_iso"].is_string())
            cloudLastSyncIso_ = j["last_sync_iso"].get<std::string>();
        SF_LOG_I("Config", "Loaded cloud settings from %s", paths::USER_CLOUD_PATH);
        return true;
    } catch (const std::exception& ex) {
        SF_LOG_W("Config", "user_cloud.json parse error: %s", ex.what());
        return false;
    }
}

void Config::reloadCloudSettings()
{
    loadUserCloud();
}

bool Config::saveUserCloud()
{
    Json root = Json::object();
    if (FileSystem::exists(paths::USER_CLOUD_PATH)) {
        try {
            root = Json::parse(FileSystem::readFile(paths::USER_CLOUD_PATH));
        } catch (...) {
            root = Json::object();
        }
    }
    root["last_sync_iso"] = cloudLastSyncIso_;

    if (!FileSystem::writeFile(paths::USER_CLOUD_PATH, root.dump(2))) {
        SF_LOG_E("Config", "Failed to save %s", paths::USER_CLOUD_PATH);
        return false;
    }
    SF_LOG_I("Config", "Saved cloud settings to %s", paths::USER_CLOUD_PATH);
    return true;
}

} // namespace sf
