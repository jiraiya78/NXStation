#include "ui/SettingsSectionView.hpp"

#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "app/Version.hpp"
#include "cloud/CloudSaveService.hpp"
#include "media/ThemeBgmPlayer.hpp"
#include "forwarder/ForwarderInstaller.hpp"
#include "launcher/NroLauncher.hpp"
#include "media/VideoPlayer.hpp"
#include "ui/CloudRestorePickView.hpp"
#include "ui/CloudSyncView.hpp"
#include "ui/CorePathsSettingsView.hpp"
#include "ui/FileBrowserView.hpp"
#include "ui/FoldersLogsView.hpp"
#include "ui/GoogleDriveLinkView.hpp"
#include "ui/LibraryScanView.hpp"
#include "ui/SettingsHelpView.hpp"
#include "ui/SettingsHelpers.hpp"
#include "ui/SystemsBrowser.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/SystemBrowserStyle.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/ThemePickerView.hpp"
#include "ui/UiSfx.hpp"
#include "ui/UiTransition.hpp"
#include "util/ActionLog.hpp"
#include "util/AppUpdater.hpp"

#include <borealis/views/dialog.hpp>
#include <borealis/views/label.hpp>

namespace sf::ui {
namespace {

brls::DetailCell* g_cloudLastRow = nullptr;

void refreshCloudSaveSection()
{
    Config::instance().reloadCloudSettings();
    if (g_cloudLastRow)
        g_cloudLastRow->setDetailText(settingsCloudLastBackupLabel());
}

bool usesListBrowser()
{
    return ThemeManager::instance().themeForcesListBrowser()
           || Config::instance().systemBrowserStyle() == SystemBrowserStyle::List;
}

} // namespace

void buildAppLauncherSettingsRows(brls::Box* listBox)
{
    std::string mode = AppState::instance().isAppletMode() ? "Applet (limited)" : "Title (full)";
    listBox->addView(makeSettingsRow("Memory Mode", mode));

    if (AppState::instance().isAppletMode()) {
        auto* warn = new brls::Label();
        warn->setText("Video and high-res art are disabled in Applet Mode.\n"
                      "Use Title Override for full features.");
        warn->setFontSize(18);
        warn->setTextColor(ThemeManager::instance().color("nxstation/muted_text"));
        listBox->addView(warn);
    }

    auto* forwarderItem = makeSettingsRow("Install Forwarder", "Home Menu icon (needs prod.keys)");
    forwarderItem->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/InstallForwarder");
        std::string error;
        if (sf::ForwarderInstaller::installNxStation(error))
            brls::Application::notify(error.empty() ? "Forwarder installed" : error);
        else
            brls::Application::notify(error.empty() ? "Forwarder install failed" : error);
        return true;
    });
    listBox->addView(forwarderItem);

    auto* rescan = makeSettingsRow("Rescan Libraries", "Tap to refresh");
    rescan->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/RescanAll");
        LibraryScanView::presentAll(true);
        return true;
    });
    listBox->addView(rescan);

    auto* updateRow = makeSettingsRow("Check for Updates", std::string("GitHub · v") + kAppVersion);
    updateRow->registerClickAction([updateRow](brls::View*) {
        SF_LOG_ACTION("Settings/CheckForUpdates");
        updateRow->setDetailText("Checking…");
        brls::Application::notify("Checking GitHub for updates…");

        AppUpdater::checkForUpdate([updateRow](AppUpdater::CheckResult result) {
            if (!result.ok) {
                updateRow->setDetailText("Check failed");
                brls::Application::notify(result.message.empty() ? result.error : result.message);
                return;
            }

            if (result.upToDate || !result.updateAvailable) {
                updateRow->setDetailText(std::string("Up to date · v") + kAppVersion);
                brls::Application::notify(result.message);
                return;
            }

            updateRow->setDetailText("v" + result.remoteVersion + " available");

            if (result.downloadUrl.empty()) {
                brls::Application::notify(result.message);
                return;
            }

            auto* panel = new brls::Box();
            panel->setAxis(brls::Axis::COLUMN);
            panel->setPadding(24, 28, 28, 28);
            auto* label = new brls::Label();
            label->setText(result.message +
                           "\n\nDownload the update now?\n\n"
                           "Keep the Switch awake until it finishes. When done, confirm to "
                           "close and restart NXStation.");
            label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            label->setSingleLine(false);
            label->setLineHeight(1.35f);
            panel->addView(label);

            auto* dialog = new brls::Dialog(panel);
            dialog->setCancelable(true);
            const std::string url = result.downloadUrl;
            dialog->addButton("Download", [updateRow, url] {
                updateRow->setDetailText("Downloading… 0%");
                brls::Application::notify("Downloading update…");
                AppUpdater::downloadAndInstall(
                    url,
                    [updateRow](bool ok, std::string message) {
                        if (!ok) {
                            updateRow->setDetailText("Download failed");
                            brls::Application::notify(message);
                            return;
                        }

                        updateRow->setDetailText("Restarting…");

                        auto* donePanel = new brls::Box();
                        donePanel->setAxis(brls::Axis::COLUMN);
                        donePanel->setPadding(24, 28, 28, 28);
                        auto* doneLabel = new brls::Label();
                        doneLabel->setText(
                            "Update downloaded.\n\n"
                            "NXStation will close and restart to apply it.\n\n"
                            "Press OK to continue.");
                        doneLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
                        doneLabel->setSingleLine(false);
                        doneLabel->setLineHeight(1.35f);
                        donePanel->addView(doneLabel);

                        auto* doneDialog = new brls::Dialog(donePanel);
                        doneDialog->setCancelable(false);
                        doneDialog->addButton("OK", [updateRow] {
                            std::string err;
                            const std::string path = AppUpdater::pendingOrInstalledPath();
                            if (!AppUpdater::relaunchViaNextLoad(path, err)) {
                                updateRow->setDetailText("Restart failed");
                                brls::Application::notify(err.empty() ? "Could not restart" : err);
                            }
                        });
                        doneDialog->open();
                    },
                    [updateRow](int percent) {
                        updateRow->setDetailText("Downloading… " + std::to_string(percent) + "%");
                    });
            });
            dialog->addButton("Later", [] {});
            dialog->open();
        });
        return true;
    });
    listBox->addView(updateRow);
}

void buildEmulatorSystemsSettingsRows(brls::Box* listBox)
{
    auto* raPath = makeSettingsRow("RetroArch", Config::instance().retroArchPath());
    listBox->addView(raPath);

    auto* raMenu = makeSettingsRow("Open RetroArch Menu", "Adjust cores & settings");
    raMenu->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/OpenRetroArchMenu");
        std::string error;
        if (!NroLauncher::openRetroArchMenu(error))
            brls::Application::notify(error.empty() ? "Could not open RetroArch" : error);
        return true;
    });
    listBox->addView(raMenu);

    auto* romsRow = makeSettingsRow("Custom ROMs Path", Config::instance().effectiveRomsRoot());
    romsRow->registerClickAction([romsRow](brls::View*) {
        SF_LOG_ACTION("Settings/RomsRoot");
        FileBrowserView::presentForDirectory(
            Config::instance().effectiveRomsRoot(),
            [romsRow](std::string path) {
                Config::instance().setRomsRootOverride(path);
                Config::instance().saveUserSettings();
                Config::instance().reload();
                romsRow->setDetailText(Config::instance().effectiveRomsRoot());
                brls::Application::notify("ROMs path updated — rescanning library");
                LibraryScanView::presentAll(true);
            },
            "Select ROMs Root Folder");
        return true;
    });
    listBox->addView(romsRow);

    auto* coreRow = makeSettingsRow("Browse Core Paths", "Per-system RetroArch cores");
    coreRow->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/CorePaths");
        CorePathsSettingsView::present();
        return true;
    });
    listBox->addView(coreRow);
}

void buildAppearanceControlSettingsRows(brls::Box* listBox)
{
    bool videoOn = Config::instance().videoEnabled() && AppState::instance().videoAllowed();
    auto* videoItem = makeSettingsRow("Video Previews", videoOn ? "On" : "Off");
    videoItem->registerClickAction([videoItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleVideo");
        if (AppState::instance().isAppletMode()) {
            brls::Application::notify("Video disabled in Applet Mode");
            return true;
        }
        bool next = !Config::instance().videoEnabled();
        Config::instance().setVideoEnabled(next);
        AppState::instance().video().setEnabled(next);
        const bool audioOn = next && Config::instance().videoAudioEnabled();
        AppState::instance().video().setAudioEnabled(audioOn);
        AppState::instance().video().previewAudio().setEnabled(audioOn);
        videoItem->setDetailText(next ? "On" : "Off");
        return true;
    });
    listBox->addView(videoItem);

    auto* videoDelayItem =
        makeSettingsRow("Video Preview Delay",
                        settingsVideoPreviewDelayLabel(Config::instance().videoPreviewDelaySeconds()));
    videoDelayItem->registerClickAction([videoDelayItem](brls::View*) {
        SF_LOG_ACTION("Settings/VideoPreviewDelay");
        const int next = nextSettingsVideoPreviewDelay(Config::instance().videoPreviewDelaySeconds());
        Config::instance().setVideoPreviewDelaySeconds(next);
        Config::instance().saveUserSettings();
        AppState::instance().video().setHoverDelaySeconds(Config::instance().hoverDelaySeconds());
        videoDelayItem->setDetailText(settingsVideoPreviewDelayLabel(next));
        return true;
    });
    listBox->addView(videoDelayItem);

    auto* videoAudioItem = makeSettingsRow("Video Preview Audio",
                                           Config::instance().videoAudioEnabled() ? "On" : "Off");
    videoAudioItem->registerClickAction([videoAudioItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleVideoAudio");
        if (AppState::instance().isAppletMode()) {
            brls::Application::notify("Audio disabled in Applet Mode");
            return true;
        }
        bool next = !Config::instance().videoAudioEnabled();
        Config::instance().setVideoAudioEnabled(next);
        AppState::instance().video().setAudioEnabled(next);
        AppState::instance().video().previewAudio().setEnabled(next);
        Config::instance().saveUserSettings();
        videoAudioItem->setDetailText(next ? "On" : "Off");
        return true;
    });
    listBox->addView(videoAudioItem);

    auto* videoVolItem =
        makeSettingsRow("Video Preview Volume", settingsVolumeLabel(Config::instance().videoAudioVolume()));
    videoVolItem->registerClickAction([videoVolItem](brls::View*) {
        SF_LOG_ACTION("Settings/VideoAudioVolume");
        float next = nextSettingsVolumeStep(Config::instance().videoAudioVolume());
        Config::instance().setVideoAudioVolume(next);
        AppState::instance().video().setAudioVolume(next);
        AppState::instance().video().previewAudio().setVolume(next);
        Config::instance().saveUserSettings();
        videoVolItem->setDetailText(settingsVolumeLabel(next));
        return true;
    });
    listBox->addView(videoVolItem);

    auto* navSoundItem =
        makeSettingsRow("Navigation Sound", Config::instance().navSoundEnabled() ? "On" : "Off");
    navSoundItem->registerClickAction([navSoundItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleNavSound");
        bool next = !Config::instance().navSoundEnabled();
        Config::instance().setNavSoundEnabled(next);
        setNavSoundEnabled(next);
        Config::instance().saveUserSettings();
        navSoundItem->setDetailText(next ? "On" : "Off");
        if (next)
            playNavSfx();
        return true;
    });
    listBox->addView(navSoundItem);

    auto* navVolItem =
        makeSettingsRow("Navigation Sound Volume", settingsVolumeLabel(Config::instance().navSoundVolume()));
    navVolItem->registerClickAction([navVolItem](brls::View*) {
        SF_LOG_ACTION("Settings/NavSoundVolume");
        float next = nextSettingsVolumeStep(Config::instance().navSoundVolume());
        Config::instance().setNavSoundVolume(next);
        setNavSoundVolume(next);
        Config::instance().saveUserSettings();
        navVolItem->setDetailText(settingsVolumeLabel(next));
        if (next > 0.f)
            playNavSfx();
        return true;
    });
    listBox->addView(navVolItem);

    auto* bgmItem = makeSettingsRow("Theme Music", Config::instance().bgmEnabled() ? "On" : "Off");
    bgmItem->registerClickAction([bgmItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleBgm");
        if (AppState::instance().isAppletMode()) {
            brls::Application::notify("Theme music disabled in Applet Mode");
            return true;
        }
        bool next = !Config::instance().bgmEnabled();
        Config::instance().setBgmEnabled(next);
        audio::ThemeBgmPlayer::instance().setEnabled(next);
        Config::instance().saveUserSettings();
        bgmItem->setDetailText(next ? "On" : "Off");
        return true;
    });
    listBox->addView(bgmItem);

    auto* bgmVolItem =
        makeSettingsRow("Theme Music Volume", settingsVolumeLabel(Config::instance().bgmVolume()));
    bgmVolItem->registerClickAction([bgmVolItem](brls::View*) {
        SF_LOG_ACTION("Settings/BgmVolume");
        float next = nextSettingsVolumeStep(Config::instance().bgmVolume());
        Config::instance().setBgmVolume(next);
        audio::ThemeBgmPlayer::instance().setVolume(next);
        Config::instance().saveUserSettings();
        bgmVolItem->setDetailText(settingsVolumeLabel(next));
        return true;
    });
    listBox->addView(bgmVolItem);

    auto* hideEmptyItem =
        makeSettingsRow("Hide Empty Systems", Config::instance().hideEmptySystems() ? "On" : "Off");
    hideEmptyItem->registerClickAction([hideEmptyItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleHideEmpty");
        bool next = !Config::instance().hideEmptySystems();
        Config::instance().setHideEmptySystems(next);
        Config::instance().saveUserSettings();
        hideEmptyItem->setDetailText(next ? "On" : "Off");
        brls::Application::notify(next ? "Empty systems hidden" : "Showing all systems");
        return true;
    });
    listBox->addView(hideEmptyItem);

    auto* screensaverItem =
        makeSettingsRow("Screensaver Delay", settingsScreensaverLabel(Config::instance().screensaverIdleSeconds()));
    screensaverItem->registerClickAction([screensaverItem](brls::View*) {
        SF_LOG_ACTION("Settings/ScreensaverDelay");
        int next = nextSettingsScreensaverPreset(Config::instance().screensaverIdleSeconds());
        Config::instance().setScreensaverIdleSeconds(next);
        Config::instance().saveUserSettings();
        screensaverItem->setDetailText(settingsScreensaverLabel(next));
        brls::Application::notify("Screensaver: " + settingsScreensaverLabel(next));
        return true;
    });
    listBox->addView(screensaverItem);

    auto* themeRow = makeSettingsRow("Theme", ThemeManager::instance().currentDisplayLabel());
    themeRow->registerClickAction([themeRow](brls::View*) {
        SF_LOG_ACTION("Settings/Theme");
        ThemePickerView::present([themeRow](std::string folder) {
            Config::instance().setThemeName(folder);
            ThemeManager::instance().setTheme(folder);
            Config::instance().saveUserSettings();
            SystemsBrowser::requestRefreshAfterSettings();
            themeRow->setDetailText(ThemeManager::instance().currentDisplayLabel());
            brls::Application::notify(ThemeManager::instance().currentDisplayLabel());
        });
        return true;
    });
    listBox->addView(themeRow);

    auto* gameArtRow = makeSettingsRow("Game Art", settingsGameArtLabel(Config::instance().gameArtMode()));
    gameArtRow->registerClickAction([gameArtRow](brls::View*) {
        SF_LOG_ACTION("Settings/GameArt");
        playToggleSfx();
        const GameArtMode next = Config::instance().gameArtMode() == GameArtMode::BoxArt
                                     ? GameArtMode::Thumbnail
                                     : GameArtMode::BoxArt;
        Config::instance().setGameArtMode(next);
        Config::instance().saveUserSettings();
        gameArtRow->setDetailText(settingsGameArtLabel(next));
        brls::Application::notify(settingsGameArtLabel(next));
        return true;
    });
    listBox->addView(gameArtRow);

    auto* browserStyleRow = makeSettingsRow(
        "System Browser",
        ThemeManager::instance().themeForcesListBrowser()
            ? "List (theme)"
            : systemBrowserStyleLabel(Config::instance().systemBrowserStyle()));
    browserStyleRow->registerClickAction([browserStyleRow](brls::View*) {
        SF_LOG_ACTION("Settings/SystemBrowserStyle");
        if (ThemeManager::instance().themeForcesListBrowser()) {
            brls::Application::notify("Active theme requires list layout");
            return true;
        }
        playToggleSfx();
        const SystemBrowserStyle next =
            nextSystemBrowserStyle(Config::instance().systemBrowserStyle());
        Config::instance().setSystemBrowserStyle(next);
        Config::instance().saveUserSettings();
        SystemsBrowser::requestRefreshAfterSettings();
        browserStyleRow->setDetailText(systemBrowserStyleLabel(next));
        brls::Application::notify(systemBrowserStyleLabel(next));
        return true;
    });
    listBox->addView(browserStyleRow);

    auto* carouselTransitionRow =
        makeSettingsRow("Carousel Transition", carouselTransitionLabel(Config::instance().carouselTransition()));
    carouselTransitionRow->registerClickAction([carouselTransitionRow](brls::View*) {
        SF_LOG_ACTION("Settings/CarouselTransition");
        if (usesListBrowser()) {
            brls::Application::notify("Only applies to carousel layout");
            return true;
        }
        playToggleSfx();
        const CarouselTransition next = nextCarouselTransition(Config::instance().carouselTransition());
        Config::instance().setCarouselTransition(next);
        Config::instance().saveUserSettings();
        carouselTransitionRow->setDetailText(carouselTransitionLabel(next));
        brls::Application::notify(carouselTransitionLabel(next));
        return true;
    });
    if (!usesListBrowser())
        listBox->addView(carouselTransitionRow);

    auto* stickScrollRow = makeSettingsRow(
        "Right Stick Scroll", Config::instance().rightStickDescriptionScroll() ? "On" : "Off");
    stickScrollRow->registerClickAction([stickScrollRow](brls::View*) {
        SF_LOG_ACTION("Settings/RightStickScroll");
        playToggleSfx();
        const bool next = !Config::instance().rightStickDescriptionScroll();
        Config::instance().setRightStickDescriptionScroll(next);
        Config::instance().saveUserSettings();
        stickScrollRow->setDetailText(next ? "On" : "Off");
        brls::Application::notify(next ? "Right stick scroll on" : "Right stick scroll off");
        return true;
    });
    listBox->addView(stickScrollRow);

    auto* scrollbarItem =
        makeSettingsRow("List Scrollbar", Config::instance().romListScrollbar() ? "On" : "Off");
    scrollbarItem->registerClickAction([scrollbarItem](brls::View*) {
        SF_LOG_ACTION("Settings/ToggleRomListScrollbar");
        const bool next = !Config::instance().romListScrollbar();
        Config::instance().setRomListScrollbar(next);
        Config::instance().saveUserSettings();
        scrollbarItem->setDetailText(next ? "On" : "Off");
        brls::Application::notify(next ? "List scrollbar on" : "List scrollbar off");
        return true;
    });
    listBox->addView(scrollbarItem);
}

void buildCloudSaveSettingsRows(brls::Box* listBox)
{
    g_cloudLastRow = nullptr;

    auto cloudProviderLabel = []() -> std::string {
        const std::string p = Config::instance().cloudProvider();
        if (p == "google_drive")
            return "Google Drive";
        return p.empty() ? "Google Drive" : p;
    };

    auto cloudAccountStatus = []() -> std::string {
        if (sf::cloud::CloudSaveService::instance().isLinked())
            return "Linked";
        return "Not linked";
    };

    auto* cloudEnableRow =
        makeSettingsRow("Auto Cloud Save", Config::instance().cloudAutoSaveEnabled() ? "On" : "Off");
    cloudEnableRow->registerClickAction([cloudEnableRow](brls::View*) {
        SF_LOG_ACTION("Settings/CloudAutoSave");
        playToggleSfx();
        const bool next = !Config::instance().cloudAutoSaveEnabled();
        Config::instance().setCloudAutoSaveEnabled(next);
        Config::instance().saveUserSettings();
        cloudEnableRow->setDetailText(next ? "On" : "Off");
        brls::Application::notify(next ? "Cloud auto save on" : "Cloud auto save off");
        return true;
    });
    listBox->addView(cloudEnableRow);

    auto* cloudPlatformRow = makeSettingsRow("Cloud Platform", cloudProviderLabel());
    cloudPlatformRow->registerClickAction([cloudPlatformRow, cloudProviderLabel](brls::View*) {
        SF_LOG_ACTION("Settings/CloudPlatform");
        playToggleSfx();
        std::string next = Config::instance().cloudProvider();
        if (next != "google_drive")
            next = "google_drive";
        Config::instance().setCloudProvider(next);
        Config::instance().saveUserSettings();
        cloudPlatformRow->setDetailText(cloudProviderLabel());
        brls::Application::notify("Cloud platform: Google Drive");
        return true;
    });
    listBox->addView(cloudPlatformRow);

    auto* cloudAccountRow = makeSettingsRow("Account", cloudAccountStatus());
    listBox->addView(cloudAccountRow);

    auto* cloudLinkRow = makeSettingsRow("Link Google Account", "");
    cloudLinkRow->registerClickAction([cloudAccountRow, cloudAccountStatus](brls::View*) {
        SF_LOG_ACTION("Settings/CloudLink");
        GoogleDriveLinkView::present([cloudAccountRow, cloudAccountStatus](bool linked) {
            if (linked && cloudAccountRow)
                cloudAccountRow->setDetailText(cloudAccountStatus());
        });
        return true;
    });
    listBox->addView(cloudLinkRow);

    auto* cloudSyncRow = makeSettingsRow("Backup Saves to Cloud", "");
    cloudSyncRow->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/CloudBackup");
        CloudSyncView::present([](bool ok) {
            if (ok) {
                Config::instance().reloadCloudSettings();
                refreshCloudSaveSection();
            }
        });
        return true;
    });
    listBox->addView(cloudSyncRow);

    auto* cloudRestoreRow = makeSettingsRow("Cloud Restore", "Merge backup from Drive");
    cloudRestoreRow->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/CloudRestore");
        if (!sf::cloud::CloudSaveService::instance().isLinked()) {
            brls::Application::notify("Link Google account first");
            return true;
        }
        CloudRestorePickView::present();
        return true;
    });
    listBox->addView(cloudRestoreRow);

    g_cloudLastRow = makeSettingsRow("Last Backup", settingsCloudLastBackupLabel());
    listBox->addView(g_cloudLastRow);
}

void buildScraperSettingsRows(brls::Box* listBox)
{
    auto maskSecret = [](const std::string& value) -> std::string {
        if (value.empty())
            return "(not set)";
        return std::string(value.size(), '*');
    };

    auto* ssUserRow = makeSettingsRow("Website Login (ssid)", Config::instance().screenscraperUser());
    ssUserRow->registerClickAction([ssUserRow](brls::View*) {
        SF_LOG_ACTION("Settings/EditSSUser");
        std::string current = Config::instance().screenscraperUser();
        brls::Application::getImeManager()->openForText(
            [ssUserRow](std::string text) {
                Config::instance().setScreenScraperUser(text);
                Config::instance().saveUserScreenScraper();
                ssUserRow->setDetailText(text.empty() ? "(not set)" : text);
                brls::Application::notify("ScreenScraper login saved");
            },
            "ScreenScraper Website Login",
            "Your screenscraper.fr username",
            127,
            current);
        return true;
    });
    listBox->addView(ssUserRow);

    auto* ssPassRow =
        makeSettingsRow("Website Password", maskSecret(Config::instance().screenscraperPassword()));
    ssPassRow->registerClickAction([ssPassRow, maskSecret](brls::View*) {
        SF_LOG_ACTION("Settings/EditSSPassword");
        std::string current = Config::instance().screenscraperPassword();
        brls::Application::getImeManager()->openForText(
            [ssPassRow, maskSecret](std::string text) {
                Config::instance().setScreenScraperPassword(text);
                Config::instance().saveUserScreenScraper();
                ssPassRow->setDetailText(maskSecret(text));
                brls::Application::notify("ScreenScraper password saved");
            },
            "ScreenScraper Website Password",
            "Your screenscraper.fr password",
            127,
            current);
        return true;
    });
    listBox->addView(ssPassRow);
}

void buildHelpAboutSettingsRows(brls::Box* listBox)
{
    auto* helpRow = makeSettingsRow("Help — Data Folders", "Placeholders, art, gamelist");
    helpRow->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/Help");
        SettingsHelpView::present();
        return true;
    });
    listBox->addView(helpRow);

    auto* foldersRow = makeSettingsRow("Folders & Logs", "Data, settings, and log paths");
    foldersRow->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/FoldersLogs");
        FoldersLogsView::present();
        return true;
    });
    listBox->addView(foldersRow);

    auto* about = makeSettingsRow("About", std::string("v") + kAppVersion);
    about->registerClickAction([](brls::View*) {
        SF_LOG_ACTION("Settings/About");
        auto* panel = new brls::Box();
        panel->setAxis(brls::Axis::COLUMN);
        panel->setPadding(24, 28, 28, 28);
        auto* label = new brls::Label();
        label->setSingleLine(false);
        label->setFontSize(20);
        label->setLineHeight(1.4f);
        label->setText(std::string("NXStation v") + kAppVersion +
                       "\n\nCredits\n"
                       "Theme music (bgm.mp3) by Vlad Krotov from Pixabay\n"
                       "RetroArch · ScreenScraper · Borealis");
        panel->addView(label);
        auto* dialog = new brls::Dialog(panel);
        dialog->addButton("OK", []() {});
        dialog->open();
        return true;
    });
    listBox->addView(about);
}

void refreshCloudSaveSettingsSection()
{
    refreshCloudSaveSection();
}

} // namespace sf::ui
