#pragma once

#include "app/Models.hpp"
#include "ui/SystemBrowserStyle.hpp"
#include "ui/UiTransition.hpp"
#include "util/GameArt.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sf {

class Config {
public:
    static Config& instance();

    bool load(const std::string& path);
    bool reload();

    /** Copy bundled roms_config, default user_cores, and system_descriptions into settings/ when missing. */
    bool seedReferenceSettingsFiles();

    const std::vector<SystemConfig>& systems() const { return systems_; }
    const SystemConfig* findSystem(const std::string& id) const;

    std::string screenscraperSoftName() const;
    std::string screenscraperUser() const { return ssUser_; }
    std::string screenscraperPassword() const { return ssPassword_; }

    /** Website login required before scraping (ssid/sspassword from Settings). */
    bool hasScreenScraperWebsiteLogin() const;

    void setScreenScraperUser(const std::string& v) { ssUser_ = v; }
    void setScreenScraperPassword(const std::string& v) { ssPassword_ = v; }
    bool saveUserScreenScraper();

    bool videoEnabled() const { return videoEnabled_; }
    void setVideoEnabled(bool v) { videoEnabled_ = v; }

    bool videoAudioEnabled() const { return videoAudioEnabled_; }
    void setVideoAudioEnabled(bool v) { videoAudioEnabled_ = v; }

    bool navSoundEnabled() const { return navSoundEnabled_; }
    void setNavSoundEnabled(bool v) { navSoundEnabled_ = v; }

    float navSoundVolume() const { return navSoundVolume_; }
    void setNavSoundVolume(float v) { navSoundVolume_ = v; }

    float videoAudioVolume() const { return videoAudioVolume_; }
    void setVideoAudioVolume(float v) { videoAudioVolume_ = v; }

    bool bgmEnabled() const { return bgmEnabled_; }
    void setBgmEnabled(bool v) { bgmEnabled_ = v; }

    float bgmVolume() const { return bgmVolume_; }
    void setBgmVolume(float v) { bgmVolume_ = v; }

    bool hideEmptySystems() const { return hideEmptySystems_; }
    void setHideEmptySystems(bool v) { hideEmptySystems_ = v; }

    /** Offer to install the Home Menu forwarder on startup when none is found. */
    bool forwarderPromptEnabled() const { return forwarderPromptEnabled_; }
    void setForwarderPromptEnabled(bool v) { forwarderPromptEnabled_ = v; }

    /** Screensaver idle timeout in seconds; 0 disables. Default 120 (2 minutes). */
    int screensaverIdleSeconds() const { return screensaverIdleSeconds_; }
    void setScreensaverIdleSeconds(int v) { screensaverIdleSeconds_ = v; }

    std::string themeName() const { return themeName_; }
    void setThemeName(const std::string& v) { themeName_ = v; }

    GameArtMode gameArtMode() const { return gameArtMode_; }
    void setGameArtMode(GameArtMode v) { gameArtMode_ = v; }

    CarouselTransition carouselTransition() const { return carouselTransition_; }
    void setCarouselTransition(CarouselTransition v) { carouselTransition_ = v; }

    SystemBrowserStyle systemBrowserStyle() const { return systemBrowserStyle_; }
    void setSystemBrowserStyle(SystemBrowserStyle v) { systemBrowserStyle_ = v; }

    /** Right stick scrolls game description text in the list and metadata popup. */
    bool rightStickDescriptionScroll() const { return rightStickDescriptionScroll_; }
    void setRightStickDescriptionScroll(bool v) { rightStickDescriptionScroll_ = v; }

    /** Show the ROM list scroll indicator bar. */
    bool romListScrollbar() const { return romListScrollbar_; }
    void setRomListScrollbar(bool v) { romListScrollbar_ = v; }

    /** Seconds to wait on a game before starting the video preview (1–6). */
    int videoPreviewDelaySeconds() const { return videoPreviewDelaySeconds_; }
    void setVideoPreviewDelaySeconds(int v) { videoPreviewDelaySeconds_ = v; }

    /** Manual viewer layout: "cover_spread" or "single_page". */
    std::string manualLayoutMode() const { return manualLayoutMode_; }
    void setManualLayoutMode(const std::string& v) { manualLayoutMode_ = v; }

    bool scrapeBoxArt() const { return scrapeBoxArt_; }
    void setScrapeBoxArt(bool v) { scrapeBoxArt_ = v; }
    bool scrapeThumbnail() const { return scrapeThumbnail_; }
    void setScrapeThumbnail(bool v) { scrapeThumbnail_ = v; }
    bool scrapeVideo() const { return scrapeVideo_; }
    void setScrapeVideo(bool v) { scrapeVideo_ = v; }
    bool scrapeManual() const { return scrapeManual_; }
    void setScrapeManual(bool v) { scrapeManual_ = v; }

    bool scrapeOptimizedMedia() const { return scrapeOptimizedMedia_; }
    void setScrapeOptimizedMedia(bool v) { scrapeOptimizedMedia_ = v; }

    /** Cloud auto-save of RetroArch saves/states to remote storage. */
    bool cloudAutoSaveEnabled() const { return cloudAutoSaveEnabled_; }
    void setCloudAutoSaveEnabled(bool v) { cloudAutoSaveEnabled_ = v; }

    /** Cloud provider id, e.g. "google_drive". */
    std::string cloudProvider() const { return cloudProvider_; }
    void setCloudProvider(const std::string& v) { cloudProvider_ = v; }

    std::string cloudLastSyncIso() const { return cloudLastSyncIso_; }
    void setCloudLastSyncIso(const std::string& v) { cloudLastSyncIso_ = v; }

    /** Re-read cloud metadata (e.g. last sync time) from user_cloud.json. */
    void reloadCloudSettings();

    bool saveUserCloud();

    float hoverDelaySeconds() const { return static_cast<float>(videoPreviewDelaySeconds_); }
    size_t textureCacheLimit() const { return textureCacheLimit_; }
    size_t requestDelayMs() const { return requestDelayMs_; }

    std::string retroArchPath() const { return retroArchPath_; }

    /** Effective core path (user override or default from roms_config). */
    std::string coreFor(const std::string& systemId) const;

    void setSystemCore(const std::string& systemId, const std::string& corePath);
    bool saveUserCores();
    bool saveUserSettings();

    /** True after the first full library scan has completed (stored in user_settings.json). */
    bool libraryScanCompleted() const { return libraryScanCompleted_; }
    void setLibraryScanCompleted(bool v) { libraryScanCompleted_ = v; }

    /** Default ROM root baked into roms_config.json entries (e.g. "sdmc:/roms/nes"). */
    static std::string defaultRomsRoot() { return "sdmc:/roms"; }

    /** Custom ROM root override so NXStation can coexist with other frontends (e.g. Tico)
     *  that use a different roms folder. Empty means use the default from roms_config.json. */
    std::string romsRootOverride() const { return romsRootOverride_; }
    void setRomsRootOverride(const std::string& v);

    /** Effective ROM root currently in use for display purposes. */
    std::string effectiveRomsRoot() const
    {
        return romsRootOverride_.empty() ? defaultRomsRoot() : romsRootOverride_;
    }

private:
    void applyRomsRootOverride();
    Config() = default;
    bool loadUserCores();
    bool loadUserScreenScraper();
    bool loadUserCloud();
    bool loadUserSettings();

    void mergeBundledSsSystemIds();
    /** Append systems present in romfs but missing from the loaded SD config (upgrade + user edits). */
    void mergeNewBundledSystems();

    std::vector<SystemConfig> systems_;
    std::string configPath_;

    std::string ssUser_;
    std::string ssPassword_;

    std::string retroArchPath_ = "sdmc:/switch/retroarch_switch.nro";

    std::unordered_map<std::string, std::string> userCoreOverrides_;
    std::string userCoresPath_;

    bool videoEnabled_ = true;
    bool videoAudioEnabled_ = true;
    bool navSoundEnabled_ = true;
    float navSoundVolume_ = 1.0f;
    float videoAudioVolume_ = 0.75f;
    bool bgmEnabled_ = true;
    float bgmVolume_ = 0.35f;
    bool hideEmptySystems_ = true;
    bool forwarderPromptEnabled_ = true;
    bool libraryScanCompleted_ = false;
    std::string romsRootOverride_;
    int screensaverIdleSeconds_ = 120;
    std::string themeName_ = "Vampire";
    GameArtMode gameArtMode_ = GameArtMode::BoxArt;
    CarouselTransition carouselTransition_ = CarouselTransition::Zoom;
    SystemBrowserStyle systemBrowserStyle_ = SystemBrowserStyle::Carousel;
    bool rightStickDescriptionScroll_ = true;
    bool romListScrollbar_ = false;
    int videoPreviewDelaySeconds_ = 2;
    std::string manualLayoutMode_ = "cover_spread";
    bool scrapeBoxArt_ = true;
    bool scrapeThumbnail_ = true;
    bool scrapeVideo_ = true;
    bool scrapeManual_ = false;
    bool scrapeOptimizedMedia_ = true;
    bool cloudAutoSaveEnabled_ = false;
    std::string cloudProvider_ = "google_drive";
    std::string cloudLastSyncIso_;
    float hoverDelaySeconds_ = 1.0f;
    size_t textureCacheLimit_ = 48;
    size_t requestDelayMs_ = 350;
};

} // namespace sf
