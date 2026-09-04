#include "ui/SystemsTab.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "ui/GameListView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/SettingsTab.hpp"
#include "ui/ScreensaverView.hpp"
#include "ui/MainMenuOptionsView.hpp"
#include "ui/LibraryScanView.hpp"
#include "ui/GameSearch.hpp"
#include "ui/SystemJumpView.hpp"
#include "ui/SystemsBrowser.hpp"
#include "ui/SystemsBrowserData.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "ui/UiTransition.hpp"
#include "ui/PlaytimeScreens.hpp"
#include "ui/CloudSyncView.hpp"
#include "ui/ZoomImage.hpp"
#include "util/ActionLog.hpp"
#include "util/IdleTimer.hpp"
#include "util/NavigationState.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"
#include "util/Placeholders.hpp"
#include "util/VirtualSystems.hpp"

#include <cmath>

namespace sf::ui {

namespace {

} // namespace

SystemsTab::SystemsTab()
{
    SF_LOG_I("UI", "SystemsTab constructing");

    rebuildSystemIds();

    this->inflateFromXMLRes("xml/views/systems_carousel.xml");
    textShade_ = static_cast<OverlayGradient*>(getView("carousel/text_shade"));

    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    bgImage->setScalingType(brls::ImageScalingType::FILL);
    bgImage->setImageAlign(brls::ImageAlignment::CENTER);
    bgImageNext->setScalingType(brls::ImageScalingType::FILL);
    bgImageNext->setImageAlign(brls::ImageAlignment::CENTER);
    bgImageNext->setVisibility(brls::Visibility::GONE);

    hintsLabel->setText("← → Browse   ·   A Open   ·   Y Menu   ·   + Settings");
    setFocusable(true);

    if (!systemIds_.empty())
        showSystem(initialSystemIndex());

    applyThemeColors();

    this->registerAction(
        "Prev", brls::ControllerButton::BUTTON_LEFT, [this](brls::View*) {
            stepSystem(-1);
            return true;
        });
    this->registerAction(
        "Next", brls::ControllerButton::BUTTON_RIGHT, [this](brls::View*) {
            stepSystem(1);
            return true;
        });
    this->registerAction(
        "Prev", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            stepSystem(-1);
            return true;
        });
    this->registerAction(
        "Next", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            stepSystem(1);
            return true;
        });

    this->registerAction(
        "Open", brls::ControllerButton::BUTTON_A, [this](brls::View*) {
            if (systemIds_.empty())
                return true;
            playConfirmSfx();
            const std::string systemId = systemIds_[index_];
            SF_LOG_ACTION("Systems/Open");
            NavigationState::setLastSystem(systemId);
            brls::sync([systemId]() {
                PushedActivity::push(new GameListView(systemId));
            });
            return true;
        });

    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_START, [](brls::View*) {
            SF_LOG_ACTION("Systems/Settings");
            playConfirmSfx();
            brls::sync([]() {
                SettingsTab::open();
            });
            return true;
        });

    this->registerAction(
        "Menu", brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
            SF_LOG_ACTION("Systems/MainMenu");
            playConfirmSfx();
            MainMenuOptionsView::present([this](MainMenuAction action) {
                if (action == MainMenuAction::Search)
                    GameSearch::prompt();
                else if (action == MainMenuAction::JumpToSystem)
                    SystemJumpView::present(systemIds_, [this](std::string systemId) {
                        for (size_t i = 0; i < systemIds_.size(); ++i) {
                            if (systemIds_[i] == systemId) {
                                playNavSfx();
                                showSystem(i);
                                return;
                            }
                        }
                    });
                else if (action == MainMenuAction::ScanGames)
                    LibraryScanView::presentAll(true);
                else if (action == MainMenuAction::SyncCloud)
                    CloudSyncView::present();
                else if (action == MainMenuAction::PersonalityMetrics)
                    presentPersonalityMetrics();
                else if (action == MainMenuAction::PlaytimeAnalytics)
                    presentPlaytimeAnalytics();
                else if (action == MainMenuAction::Settings)
                    SettingsTab::open();
            });
            return true;
        });

    SF_LOG_I("UI", "SystemsTab ready");
}

void SystemsTab::requestRefreshAfterSettings()
{
    SystemsBrowser::requestRefreshAfterSettings();
}

void SystemsTab::applyThemeColors()
{
    auto& theme = ThemeManager::instance();
    titleLabel->setTextColor(theme.color("brls/text"));
    countLabel->setTextColor(theme.color("nxstation/count_text"));
    hintsLabel->setTextColor(theme.color("nxstation/hint_text"));
    if (textShade_) {
        textShade_->setVisibility(brls::Visibility::VISIBLE);
        if (theme.isLightTheme()) {
            textShade_->setColor(nvgRGB(248, 248, 252));
            textShade_->setStrength(0.82f);
        } else {
            textShade_->setColor(nvgRGB(0, 0, 0));
            textShade_->setStrength(0.78f);
        }
    }
    shadeOverlay->setVisibility(brls::Visibility::VISIBLE);
    shadeOverlay->setColor(nvgRGB(0, 0, 0));
    shadeOverlay->setAlpha(0.f);
    this->setBackgroundColor(theme.color("brls/background"));
    this->invalidate();
}

void SystemsTab::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        applyThemeColors();
    }

    if (browser::consumeSystemsDataRefresh()) {
        applyThemeColors();
        rebuildSystemIds();
        if (!systemIds_.empty())
            showSystem(index_);
        else
            showSystem(0);
    }

    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    tickIdleScreensaver();
    tickStickNavigation(delta);
    brls::Box::frame(ctx);
}

void SystemsTab::tickStickNavigation(float delta)
{
    constexpr float kDeadzone = 0.5f;
    constexpr float kInitialDelay = 0.35f;
    constexpr float kRepeatDelay = 0.15f;

    const auto& pad = brls::Application::getControllerState();
    const float x = pad.axes[brls::LEFT_X];

    if (std::fabs(x) < kDeadzone) {
        stickHeld_ = false;
        stickRepeating_ = false;
        stickDir_ = 0;
        stickTimer_ = 0.f;
        return;
    }

    const int dir = x > 0.f ? 1 : -1;
    if (!stickHeld_ || dir != stickDir_) {
        stickHeld_ = true;
        stickRepeating_ = false;
        stickDir_ = dir;
        stickTimer_ = 0.f;
        stepSystem(dir);
        return;
    }

    stickTimer_ += delta;
    const float threshold = stickRepeating_ ? kRepeatDelay : kInitialDelay;
    if (stickTimer_ >= threshold) {
        stickTimer_ = 0.f;
        stickRepeating_ = true;
        stepSystem(dir);
    }
}

brls::View* SystemsTab::create()
{
    return new SystemsTab();
}

void SystemsTab::rebuildSystemIds()
{
    browser::rebuildSystemIds(systemIds_);
    if (index_ >= systemIds_.size())
        index_ = systemIds_.empty() ? 0 : systemIds_.size() - 1;
}

size_t SystemsTab::initialSystemIndex() const
{
    return browser::initialSystemIndex(systemIds_);
}

void SystemsTab::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    idleTimer_.reset();
    applyThemeColors();
    rebuildSystemIds();
    if (!systemIds_.empty())
        showSystem(index_);
    brls::Application::giveFocus(this);
}

void SystemsTab::tickIdleScreensaver()
{
    const int idleSec = Config::instance().screensaverIdleSeconds();
    if (idleSec <= 0) {
        idleTimer_.reset();
        return;
    }

    if (ScreensaverView::isActive()) {
        idleTimer_.reset();
        return;
    }

    idleTimer_.pollController();
    if (!idleTimer_.idleSeconds(static_cast<float>(idleSec)))
        return;

    idleTimer_.reset();
    ScreensaverView::present();
}

void SystemsTab::setSystemBackground(brls::Image* image, const std::string& systemId)
{
    if (std::string custom = browser::resolveSystemBackground(systemId); !custom.empty()) {
        SF_LOG_I("UI", "Carousel bg: %s", custom.c_str());
        image->setImageFromFile(custom);
    } else {
        browser::applySystemPlaceholder(image);
    }
}

void SystemsTab::updateSystemLabels(size_t index)
{
    if (systemIds_.empty())
        return;

    index = index % systemIds_.size();
    const std::string& id = systemIds_[index];
    titleLabel->setText(browser::systemDisplayName(id));
    countLabel->setText(browser::systemGameCountLabel(id));
}

void SystemsTab::resetBackgroundTransform()
{
    bgImage->setTranslationX(0.f);
    bgImageNext->setTranslationX(0.f);
    bgImage->setAlpha(1.f);
    bgImageNext->setAlpha(1.f);
    setImageZoom(bgImage, 1.f);
    setImageZoom(bgImageNext, 1.f);
    bgImageNext->setVisibility(brls::Visibility::GONE);
}

void SystemsTab::showSystem(size_t index)
{
    if (systemIds_.empty()) {
        titleLabel->setText("No Systems");
        countLabel->setText("0 games");
        browser::applySystemPlaceholder(bgImage);
        resetBackgroundTransform();
        return;
    }

    index_ = index % systemIds_.size();
    updateSystemLabels(index_);
    setSystemBackground(bgImage, systemIds_[index_]);
    resetBackgroundTransform();
    this->invalidate();
}

void SystemsTab::stepSystem(int delta)
{
    if (systemIds_.empty() || transitioning_)
        return;
    const int n = static_cast<int>(systemIds_.size());
    int next = static_cast<int>(index_) + delta;
    next = (next % n + n) % n;
    if (static_cast<size_t>(next) == index_)
        return;
    playNavSfx();
    beginSystemTransition(static_cast<size_t>(next), delta > 0 ? 1 : -1);
}

void SystemsTab::beginSystemTransition(size_t nextIndex, int direction)
{
    switch (Config::instance().carouselTransition()) {
    case CarouselTransition::Slide:
        beginSystemSlide(nextIndex, direction);
        break;
    case CarouselTransition::Crossfade:
        beginSystemCrossfade(nextIndex);
        break;
    case CarouselTransition::Zoom:
        beginSystemZoom(nextIndex);
        break;
    case CarouselTransition::None:
        index_ = nextIndex % systemIds_.size();
        updateSystemLabels(index_);
        setSystemBackground(bgImage, systemIds_[index_]);
        resetBackgroundTransform();
        titleLabel->setAlpha(1.f);
        countLabel->setAlpha(1.f);
        hintsLabel->setAlpha(1.f);
        shadeOverlay->setAlpha(0.f);
        this->invalidate();
        break;
    case CarouselTransition::Fade:
    default:
        beginSystemFade(nextIndex);
        break;
    }
}

void SystemsTab::beginSystemSlide(size_t nextIndex, int direction)
{
    transitioning_ = true;

    const float width = static_cast<float>(brls::Application::contentWidth);
    setSystemBackground(bgImageNext, systemIds_[nextIndex]);
    bgImageNext->setVisibility(brls::Visibility::VISIBLE);
    bgImageNext->setAlpha(1.f);
    setImageZoom(bgImage, 1.f);
    setImageZoom(bgImageNext, 1.f);
    bgImage->setTranslationX(0.f);
    bgImageNext->setTranslationX(direction * width);

    constexpr int kSlideMs = 220;
    slideAnim_.stop();
    slideAnim_.reset(0.f);
    slideAnim_.addStep(1.f, kSlideMs, brls::EasingFunction::quadraticOut);
    slideAnim_.setTickCallback([this, width, direction]() {
        const float t = slideAnim_.getValue();
        bgImage->setTranslationX(-direction * width * t);
        bgImageNext->setTranslationX(direction * width * (1.f - t));
        const float chrome = 1.f - t * 0.2f;
        titleLabel->setAlpha(chrome);
        countLabel->setAlpha(chrome);
        hintsLabel->setAlpha(chrome);
        this->invalidate();
    });
    slideAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        index_ = nextIndex % systemIds_.size();
        setSystemBackground(bgImage, systemIds_[index_]);
        updateSystemLabels(index_);
        bgImage->setTranslationX(0.f);
        bgImageNext->setTranslationX(0.f);
        bgImageNext->setVisibility(brls::Visibility::GONE);
        titleLabel->setAlpha(1.f);
        countLabel->setAlpha(1.f);
        hintsLabel->setAlpha(1.f);
        transitioning_ = false;
        this->invalidate();
    });
    slideAnim_.start();
}

void SystemsTab::beginSystemFade(size_t nextIndex)
{
    transitioning_ = true;

    constexpr int kFadeMs = 140;
    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kFadeMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        shadeOverlay->setAlpha(fadeAnim_.getValue());
        // Softly dim chrome with the shade so the swap feels like a dissolve.
        const float a = 1.f - fadeAnim_.getValue() * 0.35f;
        titleLabel->setAlpha(a);
        countLabel->setAlpha(a);
        hintsLabel->setAlpha(a);
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        showSystem(nextIndex);
        titleLabel->setAlpha(1.f);
        countLabel->setAlpha(1.f);
        hintsLabel->setAlpha(1.f);

        fadeAnim_.reset(1.f);
        fadeAnim_.addStep(0.f, kFadeMs, brls::EasingFunction::quadraticIn);
        fadeAnim_.setTickCallback([this]() {
            shadeOverlay->setAlpha(fadeAnim_.getValue());
            const float a = 1.f - fadeAnim_.getValue() * 0.35f;
            titleLabel->setAlpha(a);
            countLabel->setAlpha(a);
            hintsLabel->setAlpha(a);
        });
        fadeAnim_.setEndCallback([this](bool) {
            shadeOverlay->setAlpha(0.f);
            titleLabel->setAlpha(1.f);
            countLabel->setAlpha(1.f);
            hintsLabel->setAlpha(1.f);
            transitioning_ = false;
        });
        fadeAnim_.start();
    });
    fadeAnim_.start();
}

void SystemsTab::beginSystemCrossfade(size_t nextIndex)
{
    transitioning_ = true;
    setSystemBackground(bgImageNext, systemIds_[nextIndex]);
    bgImageNext->setVisibility(brls::Visibility::VISIBLE);
    bgImage->setTranslationX(0.f);
    bgImageNext->setTranslationX(0.f);
    bgImageNext->setAlpha(0.f);
    setImageZoom(bgImage, 1.f);
    setImageZoom(bgImageNext, 1.f);

    constexpr int kMs = 220;
    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        const float t = fadeAnim_.getValue();
        bgImageNext->setAlpha(t);
        shadeOverlay->setAlpha(0.f);
        this->invalidate();
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        showSystem(nextIndex);
        titleLabel->setAlpha(1.f);
        countLabel->setAlpha(1.f);
        hintsLabel->setAlpha(1.f);
        transitioning_ = false;
        this->invalidate();
    });
    fadeAnim_.start();
}

void SystemsTab::beginSystemZoom(size_t nextIndex)
{
    transitioning_ = true;
    setSystemBackground(bgImageNext, systemIds_[nextIndex]);
    bgImageNext->setVisibility(brls::Visibility::VISIBLE);
    bgImage->setTranslationX(0.f);
    bgImageNext->setTranslationX(0.f);
    bgImageNext->setAlpha(0.f);
    setImageZoom(bgImage, 1.f);
    setImageZoom(bgImageNext, 1.12f);

    constexpr int kMs = 240;
    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        const float t = fadeAnim_.getValue();
        bgImageNext->setAlpha(t);
        setImageZoom(bgImageNext, 1.12f - 0.12f * t);
        setImageZoom(bgImage, 1.f + 0.05f * t);
        shadeOverlay->setAlpha(0.f);
        this->invalidate();
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        showSystem(nextIndex);
        titleLabel->setAlpha(1.f);
        countLabel->setAlpha(1.f);
        hintsLabel->setAlpha(1.f);
        transitioning_ = false;
        this->invalidate();
    });
    fadeAnim_.start();
}

} // namespace sf::ui
