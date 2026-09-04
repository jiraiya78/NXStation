#include "ui/AspectFitImage.hpp"
#include "ui/GameDetailView.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "launcher/NroLauncher.hpp"
#include "media/ManualPages.hpp"
#include "ui/LaunchTransition.hpp"
#include "media/VideoPlayer.hpp"
#include "scraper/ScraperService.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "ui/ManualViewerView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/VideoPreviewView.hpp"
#include "util/ActionLog.hpp"
#include "util/FileSystem.hpp"
#include "ui/ScrapeProgressView.hpp"
#include "util/LastPlayed.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"
#include "util/RomTitle.hpp"

#include <borealis/views/cells/cell_detail.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf::ui {

GameDetailView::GameDetailView(sf::GameItem game, sf::SystemConfig system, bool metadataOnly)
    : game_(std::move(game))
    , system_(std::move(system))
    , metadataOnly_(metadataOnly)
{
    this->inflateFromXMLRes("xml/views/game_detail.xml");
    this->setBackgroundColor(ThemeManager::instance().color(
        metadataOnly_ ? "nxstation/dialog_bg" : "brls/background"));
    SF_LOG_I("UI", "Game detail opened: %s", game_.displayName.c_str());

    if (!metadataOnly_) {
        launchBtn->registerClickAction([this](brls::View*) {
            SF_LOG_ACTION("Detail/Launch");
            launchGame();
            return true;
        });

        scrapeBtn->registerClickAction([this](brls::View*) {
            SF_LOG_ACTION("Detail/Scrape");
            scrapeNow();
            return true;
        });

        raMenuBtn->registerClickAction([](brls::View*) {
            SF_LOG_ACTION("Detail/RetroArchMenu");
            std::string error;
            if (!NroLauncher::openRetroArchMenu(error))
                brls::Application::notify(error.empty() ? "Could not open RetroArch" : error);
            return true;
        });
        metaActions->setVisibility(brls::Visibility::GONE);
    } else {
        launchBtn->setVisibility(brls::Visibility::GONE);
        scrapeBtn->setVisibility(brls::Visibility::GONE);
        raMenuBtn->setVisibility(brls::Visibility::GONE);
        videoSlot->setVisibility(brls::Visibility::GONE);
        addMetadataActions();
    }

    if (!metadataOnly_)
        installVideoPreview();

    boxArt->setVisibility(brls::Visibility::GONE);
    thumbArt->setVisibility(brls::Visibility::GONE);

    aspectBoxArt_ = new AspectFitImage();
    aspectBoxArt_->setWidth(280);
    aspectBoxArt_->setHeight(360);
    aspectBoxArt_->setCornerRadius(8);
    boxArtFrame->addView(aspectBoxArt_);

    aspectThumbArt_ = new AspectFitImage();
    aspectThumbArt_->setWidth(280);
    aspectThumbArt_->setHeight(120);
    aspectThumbArt_->setCornerRadius(6);
    thumbFrame->addView(aspectThumbArt_);

    titleLabel->setAnimated(true);
    titleLabel->setSingleLine(false);
    // Full detail screen can take focus for its buttons; metadata popup must not —
    // only Rename/Delete are focusable so D-pad doesn't restyle the whole panel.
    this->setFocusable(!metadataOnly_);
    if (metadataOnly_) {
        descScroll->setFocusable(false);
        titleLabel->setFocusable(false);
        filenameLabel->setFocusable(false);
        metaLabel->setFocusable(false);
        descLabel->setFocusable(false);
        boxArt->setFocusable(false);
        thumbArt->setFocusable(false);
        aspectBoxArt_->setFocusable(false);
        aspectThumbArt_->setFocusable(false);
    }
    refreshUi();
}

GameDetailView::~GameDetailView() = default;

void GameDetailView::addMetadataActions()
{
    metaActions->setVisibility(brls::Visibility::VISIBLE);
    metaActions->setAxis(brls::Axis::ROW);
    metaActions->setFocusable(false);

    auto makeBtn = [this](const char* title, std::function<void()> onClick) {
        auto* btn = new brls::Button();
        btn->setText(title);
        btn->setStyle(&brls::BUTTONSTYLE_BORDERED);
        btn->setWidth(160);
        btn->setHeight(44);
        btn->setMarginRight(12);
        btn->setFocusable(true);
        btn->registerClickAction([onClick = std::move(onClick)](brls::View*) {
            onClick();
            return true;
        });
        metaActions->addView(btn);
        if (!firstMetaBtn_)
            firstMetaBtn_ = btn;
    };

    const std::string romStem = FileSystem::stemOf(game_.path);
    if (ManualPages::hasViewableManual(system_.path, romStem, game_.meta)) {
        makeBtn("Manual", [this, romStem] {
            SF_LOG_ACTION("Detail/Manual");
            ManualPages::tryOpenManual(system_.path, romStem, game_.meta, game_.displayName);
        });
    }

    makeBtn("Rename", [this] {
        SF_LOG_ACTION("Detail/Rename");
        const std::string ext = FileSystem::extensionOf(game_.path);
        const std::string oldStem = FileSystem::stemOf(game_.path);
#ifdef __SWITCH__
        SwkbdConfig config;
        if (R_FAILED(swkbdCreate(&config, 0))) {
            brls::Application::notify("Could not open keyboard");
            return;
        }
        swkbdConfigMakePresetDefault(&config);
        swkbdConfigSetHeaderText(&config, "Rename ROM");
        swkbdConfigSetSubText(&config, "Filename without extension");
        swkbdConfigSetStringLenMax(&config, 180);
        swkbdConfigSetInitialText(&config, oldStem.c_str());
        char buffer[256] = {};
        const Result rc = swkbdShow(&config, buffer, sizeof(buffer));
        swkbdClose(&config);
        if (R_FAILED(rc))
            return;
        std::string newStem(buffer);
#else
        std::string newStem = oldStem + "_renamed";
#endif
        while (!newStem.empty() && (newStem.front() == ' ' || newStem.front() == '\t'))
            newStem.erase(newStem.begin());
        while (!newStem.empty() && (newStem.back() == ' ' || newStem.back() == '\t'))
            newStem.pop_back();
        if (newStem.empty() || newStem == oldStem)
            return;
        for (char c : newStem) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
                c == '>' || c == '|') {
                brls::Application::notify("Filename contains invalid characters");
                return;
            }
        }
        const std::string newPath =
            FileSystem::join(FileSystem::parentPath(game_.path), newStem + ext);
        if (FileSystem::exists(newPath)) {
            brls::Application::notify("A file with that name already exists");
            return;
        }
        const std::string oldPath = game_.path;
        if (!FileSystem::renameFile(oldPath, newPath)) {
            brls::Application::notify("Rename failed");
            return;
        }
        if (!AppState::instance().renameGame(system_.id, oldPath, newPath)) {
            brls::Application::notify("Renamed on SD but library update failed — rescan");
            return;
        }
        game_.path = newPath;
        game_.filename = FileSystem::filenameOf(newPath);
        game_.displayName = RomTitle::fromStem(FileSystem::stemOf(newPath));
        game_.meta.romPath = newPath;
        refreshUi();
        brls::Application::notify("Renamed");
        if (onRenamed_)
            onRenamed_(oldPath, newPath);
    });

    makeBtn("Delete", [this] {
        SF_LOG_ACTION("Detail/Delete");
        if (!FileSystem::removeFile(game_.path)) {
            brls::Application::notify("Could not delete file");
            return;
        }
        const std::string path = game_.path;
        AppState::instance().removeGame(system_.id, path);
        brls::Application::notify("Deleted");
        if (onDeleted_)
            onDeleted_(path);
        if (dialog_)
            dialog_->close();
    });
}

void GameDetailView::presentMetadata(const sf::GameItem& game, const sf::SystemConfig& system,
                                     std::function<void()> onDismiss)
{
    auto* content = new GameDetailView(game, system, true);

    // Dialog backdrop centers its AppletFrame (default width 720). Widen that frame and
    // size content to match so the panel stays middle-centered (1120px content was
    // clipped and shifted right inside the 720 frame).
    constexpr float kFrameW = 1000.f;
    constexpr float kPanelW = 960.f;
    constexpr float kPanelH = 600.f;
    content->setWidth(kPanelW);
    content->setHeight(kPanelH);
    content->setCornerRadius(14);
    content->setBorderThickness(3);
    content->setBorderColor(ThemeManager::instance().color("nxstation/dialog_border"));
    content->setShadowType(brls::ShadowType::GENERIC);

    auto* dialog = FocusedMenuDialog::present(content, std::move(onDismiss));
    if (auto* applet = dialog->getAppletFrame()) {
        applet->setWidth(kFrameW);
        applet->setCornerRadius(14);
    }
    content->dialog_ = dialog;
    content->onDeleted_ = [dialog](const std::string&) {
        if (dialog)
            dialog->close();
    };

    // Only Rename/Delete accept focus; B cancels via Dialog::setCancelable(true).
    if (content->firstMetaBtn_) {
        if (auto* applet = dialog->getAppletFrame())
            applet->setLastFocusedView(content->firstMetaBtn_);
        brls::Application::giveFocus(content->firstMetaBtn_);
    }
}

void GameDetailView::installVideoPreview()
{
    videoPreview_ = new VideoPreviewView();
    videoSlot->addView(videoPreview_);
}

void GameDetailView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    if (!metadataOnly_ && !game_.meta.videoPath.empty())
        AppState::instance().video().onSelectionChanged(game_.meta.videoPath);
}

void GameDetailView::willDisappear(bool resetState)
{
    if (!metadataOnly_)
        AppState::instance().video().stop();
    brls::Box::willDisappear(resetState);
}

void GameDetailView::tickDescriptionStickScroll(float delta)
{
    if (!metadataOnly_ || !Config::instance().rightStickDescriptionScroll())
        return;

    const auto& pad = brls::Application::getControllerState();
    const float stickY = pad.axes[brls::RIGHT_Y];
    constexpr float kDeadzone = 0.2f;
    if (std::abs(stickY) <= kDeadzone)
        return;

    const float viewHeight = descScroll->getHeight();
    const float contentHeight = descLabel->getHeight();
    const float maxOffset = std::max(0.f, contentHeight - viewHeight);
    if (maxOffset <= 1.f)
        return;

    float offset = descScroll->getContentOffsetY();
    offset += stickY * 360.f * delta;
    descScroll->setContentOffsetY(std::clamp(offset, 0.f, maxOffset), false);
}

void GameDetailView::frame(brls::FrameContext* ctx)
{
    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    if (delta > 0.f)
        tickDescriptionStickScroll(delta);

    brls::Box::frame(ctx);
}

void GameDetailView::refreshUi()
{
    titleLabel->setText(game_.displayName);
    filenameLabel->setText(game_.filename.empty() ? FileSystem::filenameOf(game_.path)
                                                  : game_.filename);

    std::string metaLine;
    if (!game_.meta.developer.empty())
        metaLine += game_.meta.developer;
    if (!game_.meta.publisher.empty()) {
        if (!metaLine.empty())
            metaLine += " · ";
        metaLine += game_.meta.publisher;
    }
    if (!game_.meta.releaseDate.empty()) {
        if (!metaLine.empty())
            metaLine += " · ";
        metaLine += game_.meta.releaseDate;
    }
    if (!game_.meta.genre.empty()) {
        if (!metaLine.empty())
            metaLine += " · ";
        metaLine += game_.meta.genre;
    }
    if (metaLine.empty())
        metaLine = "No metadata";
    metaLabel->setText(metaLine);

    descLabel->setText(game_.meta.description.empty() ? "No description available."
                                                      : game_.meta.description);

    if (!game_.meta.boxArtPath.empty() && FileSystem::exists(game_.meta.boxArtPath))
        aspectBoxArt_->setImageFromFile(game_.meta.boxArtPath);
    else
        aspectBoxArt_->setImageFromRes("img/systems/" + system_.id + ".png");

    if (!game_.meta.logoPath.empty() && FileSystem::exists(game_.meta.logoPath)) {
        aspectThumbArt_->setImageFromFile(game_.meta.logoPath);
        thumbFrame->setVisibility(brls::Visibility::VISIBLE);
    } else {
        aspectThumbArt_->clearImage();
        thumbFrame->setVisibility(brls::Visibility::GONE);
    }
}

void GameDetailView::launchGame()
{
    NavigationState::update(system_.id, 0, game_.path);
    LastPlayed::instance().record(system_.id, game_.path);
    AppState::instance().rebuildVirtualSections();

    beginGameLaunch(system_, game_);
}

void GameDetailView::scrapeNow()
{
    SF_LOG_ACTION("Detail/Scrape");
    launchBtn->setState(brls::ButtonState::DISABLED);
    scrapeBtn->setState(brls::ButtonState::DISABLED);
    std::vector<sf::GameItem> one{game_};
    auto* view = new ScrapeProgressView(system_.id, ScrapeMode::Single, std::move(one));
    view->setOnDismiss([this]() {
        launchBtn->setState(brls::ButtonState::ENABLED);
        scrapeBtn->setState(brls::ButtonState::ENABLED);
    });
    PushedActivity::push(new ScrapeProgressActivity(view));
}

} // namespace sf::ui
