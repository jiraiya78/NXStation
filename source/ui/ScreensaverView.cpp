#include "ui/ScreensaverView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/LaunchTransition.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "ui/VideoPreviewView.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "media/VideoPlayer.hpp"
#include "util/FileSystem.hpp"
#include "util/LastPlayed.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"
#include "util/VirtualSystems.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <random>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf::ui {

namespace {

// A dismissed screensaver must not re-arm on the same frame the host view resumes ticking.
constexpr double kDismissCooldownSeconds = 2.0;

// Give up on a clip that never reports playback (missing codec, unreadable file).
constexpr float kPlaybackStartTimeout = 8.f;

std::atomic<bool> gScreensaverActive{false};
std::atomic<long long> gDismissedAtUs{0};

uint32_t hardwareRandomU32()
{
#ifdef __SWITCH__
    uint32_t seed = 0;
    if (R_SUCCEEDED(splInitialize())) {
        splGetRandomBytes(&seed, sizeof(seed));
        splExit();
    }
    return seed;
#else
    return static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

std::mt19937 makeScreensaverRng()
{
    const uint32_t seed =
        hardwareRandomU32()
        ^ static_cast<uint32_t>(brls::getCPUTimeUsec())
        ^ static_cast<uint32_t>(
              std::chrono::steady_clock::now().time_since_epoch().count());
    return std::mt19937(seed ? seed : 1u);
}

std::string systemDisplayName(const std::string& systemId)
{
    if (isVirtualSystemId(systemId))
        return virtualSystemDisplayName(systemId);
    if (const SystemConfig* sys = Config::instance().findSystem(systemId))
        return sys->name;
    return systemId;
}

} // namespace

ScreensaverView::ScreensaverView()
{
    auto& theme = ThemeManager::instance();
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setBackgroundColor(theme.color("brls/background"));

    backgroundImage_ = new brls::Image();
    backgroundImage_->setScalingType(brls::ImageScalingType::FILL);
    backgroundImage_->setPositionType(brls::PositionType::ABSOLUTE);
    backgroundImage_->setPositionTop(0.f);
    backgroundImage_->setPositionLeft(0.f);
    backgroundImage_->setWidthPercentage(100.f);
    backgroundImage_->setHeightPercentage(100.f);
    backgroundImage_->setVisibility(brls::Visibility::GONE);
    this->addView(backgroundImage_);

    overlayBox_ = new brls::Box();
    overlayBox_->setPositionType(brls::PositionType::ABSOLUTE);
    overlayBox_->setPositionTop(0.f);
    overlayBox_->setPositionLeft(0.f);
    overlayBox_->setWidthPercentage(100.f);
    overlayBox_->setHeightPercentage(100.f);
    overlayBox_->setVisibility(brls::Visibility::GONE);
    this->addView(overlayBox_);

    // Without focus the host view keeps it, and its highlight shows through the screensaver.
    this->setFocusable(true);
    this->setHideHighlight(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    auto* center = new brls::Box();
    center->setAxis(brls::Axis::COLUMN);
    center->setGrow(1.0f);
    center->setAlignItems(brls::AlignItems::CENTER);
    center->setJustifyContent(brls::JustifyContent::FLEX_START);
    center->setPadding(48, 24, 0, 24);

    videoPreview_ = new VideoPreviewView();
    center->addView(videoPreview_);
    this->addView(center);

    auto* bottom = new brls::Box();
    bottom->setAxis(brls::Axis::COLUMN);
    bottom->setAlignItems(brls::AlignItems::CENTER);
    bottom->setPadding(16, 24, 40, 24);

    titleLabel_ = new brls::Label();
    titleLabel_->setFontSize(30);
    titleLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    titleLabel_->setTextColor(theme.color("brls/text"));
    bottom->addView(titleLabel_);

    systemLabel_ = new brls::Label();
    systemLabel_->setFontSize(20);
    systemLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    systemLabel_->setTextColor(theme.color("nxstation/detail_text"));
    systemLabel_->setMarginTop(6);
    bottom->addView(systemLabel_);

    auto* hint = new brls::Label();
    hint->setFontSize(15);
    hint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    hint->setTextColor(theme.color("nxstation/hint_text"));
    hint->setMarginTop(14);
    hint->setText("A Play   ·   any other button returns");
    hintLabel_ = hint;
    bottom->addView(hint);

    this->addView(bottom);
    applyThemeColors();
}

ScreensaverView::~ScreensaverView()
{
    *alive_ = false;
}

bool ScreensaverView::isShowing()
{
    return gScreensaverActive.load();
}

bool ScreensaverView::isActive()
{
    if (gScreensaverActive.load())
        return true;

    const long long dismissedAt = gDismissedAtUs.load();
    if (dismissedAt == 0)
        return false;

    const double since =
        static_cast<double>(brls::getCPUTimeUsec() - dismissedAt) / 1000000.0;
    return since < kDismissCooldownSeconds;
}

void ScreensaverView::present(std::function<void()> onDismiss)
{
    if (isActive())
        return;

    gScreensaverActive = true;
    auto* view = new ScreensaverView();
    view->setOnDismiss(std::move(onDismiss));
    PushedActivity::push(new ScreensaverActivity(view));
}

void ScreensaverView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    gScreensaverActive = true;
    *alive_ = true;
    lastFrameTime_ = 0;
    wasPlaying_ = false;
    hasPick_ = false;
    sinceSelection_ = 0.f;

    auto& video = AppState::instance().video();
    savedHoverDelay_ = Config::instance().hoverDelaySeconds();
    savedLoopPlayback_ = video.loopPlayback();
    video.setHoverDelaySeconds(0.f);
    video.setLoopPlayback(false);
    video.stop();

    shuffleBag_.clear();
    shuffleIndex_ = 0;
    lastPath_.clear();
    refillShuffleBag();
    pickRandomGame();
    brls::Application::giveFocus(this);
    applyThemeColors();
}

void ScreensaverView::applyThemeColors()
{
    auto& theme = ThemeManager::instance();
    this->setBackgroundColor(theme.color("brls/background"));
    titleLabel_->setTextColor(theme.color("brls/text"));
    systemLabel_->setTextColor(theme.color("nxstation/detail_text"));
    hintLabel_->setTextColor(theme.color("nxstation/hint_text"));

    const std::string imagePath = theme.screensaverImagePath();
    if (imagePath != screensaverImagePath_) {
        screensaverImagePath_ = imagePath;
        if (!imagePath.empty()) {
            backgroundImage_->setImageFromFile(imagePath);
            backgroundImage_->setVisibility(brls::Visibility::VISIBLE);
            overlayBox_->setVisibility(brls::Visibility::VISIBLE);
        } else {
            backgroundImage_->setVisibility(brls::Visibility::GONE);
            overlayBox_->setVisibility(brls::Visibility::GONE);
        }
    }

    NVGcolor overlay = theme.hasColor("nxstation/screensaver_overlay")
                           ? theme.color("nxstation/screensaver_overlay")
                           : theme.color("brls/backdrop");
    overlayBox_->setBackgroundColor(overlay);
    themeGenApplied_ = theme.uiGeneration();
    this->invalidate();
}

void ScreensaverView::willDisappear(bool resetState)
{
    gScreensaverActive = false;
    gDismissedAtUs = static_cast<long long>(brls::getCPUTimeUsec());
    *alive_ = false;

    auto& video = AppState::instance().video();
    video.stop();
    video.setHoverDelaySeconds(savedHoverDelay_);
    video.setLoopPlayback(savedLoopPlayback_);

    if (onDismiss_) {
        auto cb = std::move(onDismiss_);
        onDismiss_ = nullptr;
        brls::sync(std::move(cb));
    }

    brls::Box::willDisappear(resetState);
}

void ScreensaverView::dismiss()
{
    gScreensaverActive = false;
    gDismissedAtUs = static_cast<long long>(brls::getCPUTimeUsec());
    brls::Application::popActivity(brls::TransitionAnimation::FADE);
}

void ScreensaverView::refillShuffleBag()
{
    shuffleBag_.clear();

    auto collect = [&](const std::string& systemId) {
        for (const auto& game : AppState::instance().gamesFor(systemId)) {
            if (game.meta.videoPath.empty() || !FileSystem::exists(game.meta.videoPath))
                continue;
            Pick p;
            p.systemId = game.systemId.empty() ? systemId : game.systemId;
            p.game = game;
            shuffleBag_.push_back(std::move(p));
        }
    };

    for (const auto& sys : Config::instance().systems())
        collect(sys.id);

    if (shuffleBag_.empty()) {
        hasPick_ = false;
        titleLabel_->setText("No preview videos found");
        systemLabel_->setText("Scrape videos first, or press B to return");
        return;
    }

    std::mt19937 rng = makeScreensaverRng();
    std::shuffle(shuffleBag_.begin(), shuffleBag_.end(), rng);
    shuffleIndex_ = shuffleBag_.empty() ? 0
                                        : static_cast<size_t>(rng() % shuffleBag_.size());
}

void ScreensaverView::pickRandomGame()
{
    if (shuffleBag_.empty())
        refillShuffleBag();
    if (shuffleBag_.empty())
        return;

    size_t index = 0;
    if (shuffleBag_.size() == 1) {
        index = 0;
    } else {
        std::mt19937 rng = makeScreensaverRng();
        index = static_cast<size_t>(rng() % shuffleBag_.size());
        for (int attempt = 0; attempt < 8 && shuffleBag_[index].game.path == lastPath_;
             ++attempt) {
            index = static_cast<size_t>(rng() % shuffleBag_.size());
        }
    }

    current_ = shuffleBag_[index];
    shuffleIndex_ = index + 1;
    lastPath_ = current_.game.path;
    hasPick_ = true;
    showCurrent();
}

void ScreensaverView::showCurrent()
{
    if (!hasPick_)
        return;

    titleLabel_->setText(current_.game.displayName);
    systemLabel_->setText(systemDisplayName(current_.systemId));

    wasPlaying_ = false;
    sinceSelection_ = 0.f;

    auto& video = AppState::instance().video();
    video.stop();
    video.setLoopPlayback(false);
    video.onSelectionChanged(current_.game.meta.videoPath);
}

void ScreensaverView::tickPlayback(float delta)
{
    if (!hasPick_)
        return;

    const bool playing = AppState::instance().video().playing();

    if (playing) {
        wasPlaying_ = true;
        sinceSelection_ = 0.f;
        return;
    }

    if (wasPlaying_) {
        pickRandomGame();
        return;
    }

    sinceSelection_ += delta;
    if (sinceSelection_ >= kPlaybackStartTimeout) {
        SF_LOG_W("UI", "Screensaver clip never started: %s",
                 current_.game.meta.videoPath.c_str());
        pickRandomGame();
    }
}

void ScreensaverView::frame(brls::FrameContext* ctx)
{
    if (videoPreview_)
        videoPreview_->pump(ctx);

    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    tickPlayback(delta);

    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_)
        applyThemeColors();

    brls::Box::frame(ctx);
}

void ScreensaverView::launchCurrent()
{
    if (!hasPick_) {
        dismiss();
        return;
    }

    const SystemConfig* sys = Config::instance().findSystem(current_.systemId);
    if (!sys) {
        brls::Application::notify("Unknown system: " + current_.systemId);
        dismiss();
        return;
    }

    const sf::GameItem game = current_.game;
    const std::string systemId = current_.systemId;

    size_t gameIndex = 0;
    const auto& stored = AppState::instance().gamesFor(systemId);
    for (size_t i = 0; i < stored.size(); ++i) {
        if (stored[i].path == game.path) {
            gameIndex = i;
            break;
        }
    }

    NavigationState::update(systemId, gameIndex, game.path);
    NavigationState::setLastSystem(systemId);
    LastPlayed::instance().record(systemId, game.path);
    AppState::instance().rebuildVirtualSections();

    AppState::instance().video().stop();

    beginGameLaunch(*sys, game);
}

void ScreensaverActivity::onContentAvailable()
{
    if (!view_)
        return;

    view_->registerAction(
        "Play", brls::ControllerButton::BUTTON_A,
        [this](brls::View*) {
            playConfirmSfx();
            view_->launchCurrent();
            return true;
        },
        false, false, brls::SOUND_NONE);

    // Every remaining button leaves the screensaver.
    const brls::ControllerButton dismissButtons[] = {
        brls::ControllerButton::BUTTON_B,
        brls::ControllerButton::BUTTON_X,
        brls::ControllerButton::BUTTON_Y,
        brls::ControllerButton::BUTTON_UP,
        brls::ControllerButton::BUTTON_DOWN,
        brls::ControllerButton::BUTTON_LEFT,
        brls::ControllerButton::BUTTON_RIGHT,
        brls::ControllerButton::BUTTON_NAV_UP,
        brls::ControllerButton::BUTTON_NAV_DOWN,
        brls::ControllerButton::BUTTON_NAV_LEFT,
        brls::ControllerButton::BUTTON_NAV_RIGHT,
        brls::ControllerButton::BUTTON_LB,
        brls::ControllerButton::BUTTON_RB,
        brls::ControllerButton::BUTTON_LT,
        brls::ControllerButton::BUTTON_RT,
        brls::ControllerButton::BUTTON_START,
        brls::ControllerButton::BUTTON_BACK,
        brls::ControllerButton::BUTTON_LSB,
        brls::ControllerButton::BUTTON_RSB,
    };

    for (auto btn : dismissButtons) {
        view_->registerAction(
            "Back", btn,
            [this](brls::View*) {
                view_->dismiss();
                return true;
            },
            false, false, brls::SOUND_NONE);
    }

    brls::Application::giveFocus(view_);
}

} // namespace sf::ui
