#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "media/TextureCache.hpp"
#include "media/VideoPlayer.hpp"
#include "media/SwitchAudout.hpp"
#include "scraper/HttpClient.hpp"
#include "ui/SystemsBrowser.hpp"
#include "ui/GameListView.hpp"
#include "ui/LibraryScanView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/Theme.hpp"
#include "ui/UiSfx.hpp"
#include "util/CrashHandler.hpp"
#include "util/AppUpdater.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"
#include "forwarder/ForwarderInstaller.hpp"
#include "launcher/ReturnChain.hpp"
#include "cloud/CloudSaveService.hpp"
#include "analytics/PlaySessionTracker.hpp"

#include <borealis/views/dialog.hpp>
#include <cstdlib>

#ifdef __SWITCH__
#include <switch.h>
#endif

class MainActivity : public brls::Activity {
public:
    brls::View* createContentView() override
    {
        return sf::ui::SystemsBrowser::create();
    }
};

static void maybePromptForwarderInstall();

static void pushMainAndRestoreNavigation(const sf::SavedNavigation& nav)
{
    brls::Application::pushActivity(new MainActivity(), brls::TransitionAnimation::FADE);
    sf::Logger::bootMark("main:activity_pushed");

    sf::analytics::PlaySessionTracker::commitPendingSession();

    if (nav.shouldRestore)
        sf::cloud::CloudSaveService::instance().maybeAutoSyncAfterReturn();

    if (nav.shouldRestore && !nav.systemId.empty()) {
        size_t focus = nav.gameIndex;
        const auto& games = sf::AppState::instance().gamesFor(nav.systemId);
        if (!nav.romPath.empty()) {
            for (size_t i = 0; i < games.size(); ++i) {
                if (games[i].path == nav.romPath) {
                    focus = i;
                    break;
                }
            }
        }
        if (!games.empty()) {
            if (focus >= games.size())
                focus = 0;
            SF_LOG_I("Main", "Restoring navigation: %s focus=%zu", nav.systemId.c_str(), focus);
            brls::sync([nav, focus]() {
                auto* view = new sf::ui::GameListView(nav.systemId, focus);
                brls::Application::pushActivity(new sf::ui::PushedActivity(view),
                                                brls::TransitionAnimation::FADE);
            });
        } else {
            SF_LOG_W("Main", "Restore skipped — no games for %s", nav.systemId.c_str());
        }
    }
}

static void finishLibraryScanAndContinue(const sf::SavedNavigation& nav)
{
    brls::sync([nav]() {
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        pushMainAndRestoreNavigation(nav);
        if (!nav.shouldRestore)
            maybePromptForwarderInstall();
    });
}

/** Empty host so applet-mode dialogs can close without triggering the global exit prompt. */
class AppletGateActivity : public brls::Activity {
public:
    brls::View* createContentView() override
    {
        auto* box = new brls::Box();
        box->setGrow(1.0f);
        return box;
    }
};

static void showAppletModeExitDialog()
{
    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setPadding(24, 28, 28, 28);

    auto* label = new brls::Label();
    label->setText(
        "NXStation requires Title Override mode.\n\n"
        "Applet Mode only gets a small slice of RAM, so video previews, artwork "
        "caching and game launching cannot work.\n\n"
        "How to start in Title Override:\n"
        "1. Go to the Switch Home Menu\n"
        "2. Hold R\n"
        "3. Keep holding R and press A on any installed game\n"
        "4. The Homebrew Menu opens in Title Override mode\n"
        "5. Start NXStation from there\n\n"
        "To install a Home Menu forwarder, use Settings → Install Forwarder "
        "after starting NXStation in Title Override mode.");
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setSingleLine(false);
    label->setLineHeight(1.35f);
    panel->addView(label);

    auto* dialog = new brls::Dialog(panel);
    dialog->setCancelable(false);
    dialog->addButton("Close", [] {
        if (brls::Application::getActivitiesStack().size() > 1) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE, [] {
                brls::Application::quit();
            });
        } else {
            brls::Application::quit();
        }
    });
    dialog->open();
}

static void showForwarderInstallDialog()
{
    const bool hasKeys = sf::ForwarderInstaller::keysAvailable();

    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setPadding(24, 28, 28, 28);

    auto* label = new brls::Label();
    if (hasKeys) {
        label->setText(
            "No NXStation icon was found on the Home Menu.\n\n"
            "Installing a forwarder adds an NXStation icon to the Home Menu. Starting "
            "NXStation from that icon also means games return to NXStation when you "
            "close them, instead of dropping you into the Homebrew Menu.\n\n"
            "Your hbmenu.nro is not modified.\n\n"
            "Install the forwarder now?");
    } else {
        label->setText(
            "No NXStation icon was found on the Home Menu.\n\n"
            "A forwarder adds an NXStation icon to the Home Menu and makes games return "
            "to NXStation when you close them.\n\n"
            "Installing one needs your console keys at:\n"
            "sdmc:/switch/prod.keys\n\n"
            "Copy that file, then use Settings → Install Forwarder.");
    }
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setSingleLine(false);
    label->setLineHeight(1.35f);
    panel->addView(label);

    auto* dialog = new brls::Dialog(panel);
    dialog->setCancelable(true);

    const auto dontAskAgain = [] {
        sf::Config::instance().setForwarderPromptEnabled(false);
        sf::Config::instance().saveUserSettings();
    };

    if (hasKeys) {
        dialog->addButton("Install Now", [] {
            std::string message;
            const bool ok = sf::ForwarderInstaller::installNxStation(message);
            brls::Application::notify(
                message.empty() ? (ok ? "Forwarder installed" : "Forwarder install failed")
                                : message);
        });
        dialog->addButton("Not Now", [] {});
        dialog->addButton("Don't Ask Again", dontAskAgain);
    } else {
        dialog->addButton("OK", [] {});
        dialog->addButton("Don't Ask Again", dontAskAgain);
    }

    dialog->open();
}

static void maybePromptForwarderInstall()
{
    if (!sf::Config::instance().forwarderPromptEnabled())
        return;
    if (sf::ForwarderInstaller::isForwarderInstalled())
        return;

    showForwarderInstallDialog();
}

int main(int argc, char* argv[])
{
    sf::Logger::bootMark("main:enter");
    // One-shot trim before logging starts — cheap when under 512 KiB (stat only).
    sf::Logger::trimLogIfNeeded();
    sf::Logger::instance().setLevel(sf::LogLevel::Debug);

    // Two-phase self-update: if NXStation_update.nro is waiting and APP_NRO is locked,
    // set envSetNextLoad and exit immediately so hbloader runs the new build.
    if (sf::AppUpdater::bootstrapApply(argc, argv) ==
        sf::AppUpdater::BootstrapResult::ExitForRelaunch) {
        sf::Logger::bootMark("main:exit_for_update_relaunch");
        SF_LOG_I("Main", "Exiting for update relaunch");
        sf::Logger::instance().flush();
        return EXIT_SUCCESS;
    }

    sf::CrashHandler::install();

    sf::Logger::bootMark("main:before_borealis_init");
    if (!brls::Application::init()) {
        sf::Logger::bootMark("main:borealis_init_failed");
        SF_LOG_E("Main", "Unable to init Borealis");
        return 1;
    }

    sf::Logger::bootMark("main:before_create_window");
    brls::Application::createWindow("NXStation");
    sf::ui::applyModernTheme();
    sf::ui::initUiSfx();
    brls::Application::setGlobalQuit(false);

    sf::Logger::bootMark("main:window_ready");

    if (!sf::AppState::instance().initialize()) {
        sf::Logger::bootMark("main:config_failed");
        brls::Application::notify("Failed to load configuration");
    } else {
        sf::Logger::bootMark("main:appstate_ready");
    }

    SF_LOG_I("Main", "NXStation starting");

    // Older builds swapped sdmc:/hbmenu.nro for NXStation; put the real one back.
    if (sf::hbmenuWasReplaced()) {
        std::string restoreError;
        if (sf::restoreHbmenuBackup(restoreError))
            SF_LOG_I("Main", "Restored original hbmenu.nro");
        else
            SF_LOG_W("Main", "hbmenu restore: %s", restoreError.c_str());
    }

#ifdef __SWITCH__
    if (envHasNextLoad()) {
        Result last = envGetLastLoadResult();
        if (R_FAILED(last))
            SF_LOG_W("Main", "Previous NRO load failed (0x%x)", static_cast<unsigned>(last));
    }
#endif

    if (sf::AppState::instance().isAppletMode()) {
        brls::Application::pushActivity(new AppletGateActivity());
        showAppletModeExitDialog();
    } else {
        const sf::SavedNavigation nav = sf::NavigationState::takePendingRestore();
        const bool needsInitialScan =
            !nav.shouldRestore && !sf::Config::instance().libraryScanCompleted();
        const bool needsLibraryReload =
            sf::Config::instance().libraryScanCompleted() &&
            !sf::AppState::instance().libraryInMemory();

        if (needsInitialScan || needsLibraryReload) {
            auto* scanView = new sf::ui::LibraryScanView(sf::ui::LibraryScanScope::AllSystems, {},
                                                         false);
            if (needsLibraryReload)
                scanView->setMarkLibraryCompleted(false);
            scanView->setOnComplete([nav]() { finishLibraryScanAndContinue(nav); });
            brls::Application::pushActivity(new sf::ui::LibraryScanActivity(scanView));
        } else {
            pushMainAndRestoreNavigation(nav);

            if (!nav.shouldRestore)
                brls::sync([] { maybePromptForwarderInstall(); });
        }
    }
    sf::Logger::bootMark("main:mainloop");

    while (brls::Application::mainLoop()) {
        sf::HttpClient::instance().pump();
        sf::AppState::instance().textures().pumpUploads();
        sf::AppState::instance().video().tick(1.0f / 60.0f);
    }

    sf::AppState::instance().shutdown();
    sf::audio::SwitchAudout::shutdown();
    SF_LOG_I("Main", "NXStation exiting");
    return EXIT_SUCCESS;
}
