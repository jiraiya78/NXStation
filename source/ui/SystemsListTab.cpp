#include "ui/SystemsListTab.hpp"

#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "ui/CloudSyncView.hpp"
#include "ui/GameListView.hpp"
#include "ui/GameSearch.hpp"
#include "ui/LibraryScanView.hpp"
#include "ui/MainMenuOptionsView.hpp"
#include "ui/PlaytimeScreens.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ScreensaverView.hpp"
#include "ui/SettingsTab.hpp"
#include "ui/SystemJumpView.hpp"
#include "ui/SystemsBrowserData.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "ui/UiTransition.hpp"
#include "ui/ZoomImage.hpp"
#include "util/ActionLog.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"

#include <cmath>
#include <functional>

namespace sf::ui {

namespace {

constexpr float kRowHeight = 64.f;
constexpr const char* kRowReuseId = "system_row";
constexpr const char* kEmptyHeaderReuseId = "empty_header";
constexpr int kTransitionMs = 220;

constexpr float kNavRepeatStage1Interval = 0.24f;
constexpr float kNavRepeatStage2Interval = 0.10f;
constexpr float kNavRepeatStage3Interval = 0.032f;
constexpr float kNavRepeatStage4Interval = 0.018f;
constexpr float kNavRepeatStage5Interval = 0.008f;
constexpr int kNavRepeatStage2After = 5;
constexpr int kNavRepeatStage3After = 14;
constexpr int kNavRepeatStage4After = 40;
constexpr int kNavRepeatStage5After = 100;

float repeatIntervalForCount(int repeatCount)
{
    if (repeatCount < kNavRepeatStage2After)
        return kNavRepeatStage1Interval;
    if (repeatCount < kNavRepeatStage3After)
        return kNavRepeatStage2Interval;
    if (repeatCount < kNavRepeatStage4After)
        return kNavRepeatStage3Interval;
    if (repeatCount < kNavRepeatStage5After)
        return kNavRepeatStage4Interval;
    return kNavRepeatStage5Interval;
}

void pollHold(int& holdDir, float& holdTime, float& repeatTimer, int& repeatCount, float delta,
              int negative, int positive, const std::function<void(int, bool)>& step)
{
    int dir = 0;
    if (negative && !positive)
        dir = -1;
    else if (positive && !negative)
        dir = 1;

    if (dir == 0) {
        holdDir = 0;
        holdTime = 0.f;
        repeatTimer = 0.f;
        repeatCount = 0;
        return;
    }

    if (dir != holdDir) {
        holdDir = dir;
        holdTime = 0.f;
        repeatTimer = 0.f;
        repeatCount = 0;
        step(dir, false);
        return;
    }

    holdTime += delta;
    repeatTimer += delta;
    const float interval = repeatIntervalForCount(repeatCount);
    if (repeatTimer < interval)
        return;
    repeatTimer -= interval;
    ++repeatCount;
    step(dir, true);
}

class SystemRowCell : public brls::RecyclerCell {
public:
    SystemRowCell()
    {
        setAxis(brls::Axis::ROW);
        setAlignItems(brls::AlignItems::CENTER);
        setPaddingLeft(4);
        setPaddingRight(8);
        setHeight(kRowHeight);
        setFocusable(true);
        setFocusSound(brls::SOUND_NONE);
        setHighlightPadding(2.f);

        title_ = new brls::Label();
        title_->setGrow(1.f);
        title_->setSingleLine(true);
        title_->setAnimated(false);
        title_->setFontSize(20);
        addView(title_);

        detail_ = new brls::Label();
        detail_->setSingleLine(true);
        detail_->setAnimated(false);
        detail_->setFontSize(15);
        detail_->setMarginLeft(12);
        addView(detail_);
    }

    brls::View* getParentNavigationDecision(brls::View* from, brls::View* newFocus,
                                            brls::FocusDirection direction) override
    {
        if (direction == brls::FocusDirection::LEFT || direction == brls::FocusDirection::RIGHT)
            return this;
        return brls::RecyclerCell::getParentNavigationDecision(from, newFocus, direction);
    }

    void onFocusGained() override
    {
        brls::RecyclerCell::onFocusGained();
        applyRowSeparator();
        setHideHighlight(false);
        applyFocusTextColor(true);
        if (owner_)
            owner_->onRowFocused(getIndexPath().row);
    }

    void onFocusLost() override
    {
        brls::RecyclerCell::onFocusLost();
        applyRowSeparator();
        applyFocusTextColor(false);
    }

    void prepareForReuse() override
    {
        setHideHighlight(true);
        owner_ = nullptr;
        applyRowSeparator();
    }

    void bind(SystemsListTab* owner, const std::string& title, const std::string& detail,
              NVGcolor color, NVGcolor detailColor)
    {
        title_->setText(title);
        idleTextColor_ = color;
        title_->setTextColor(color);
        detail_->setText(detail);
        detailColor_ = detailColor;
        detail_->setTextColor(detailColor);
        owner_ = owner;
        applyRowSeparator();
        if (brls::Application::getCurrentFocus() == this) {
            setHideHighlight(false);
            applyFocusTextColor(true);
        }
    }

private:
    void applyRowSeparator()
    {
        const bool hideLine =
            focused && brls::Application::getInputType() != brls::InputType::TOUCH;
        setLineColor(hideLine ? brls::TRANSPARENT
                              : ThemeManager::instance().color("brls/sidebar/separator"));
    }

    void applyFocusTextColor(bool focused)
    {
        const NVGcolor title = focused ? ThemeManager::instance().selectionTextColor(idleTextColor_)
                                       : idleTextColor_;
        title_->setTextColor(title);
        if (detail_)
            detail_->setTextColor(focused ? title : detailColor_);
    }

    SystemsListTab* owner_ = nullptr;
    brls::Label* title_ = nullptr;
    brls::Label* detail_ = nullptr;
    NVGcolor idleTextColor_{0, 0, 0, 0};
    NVGcolor detailColor_{0, 0, 0, 0};
};

class SystemRowDataSource : public brls::RecyclerDataSource {
public:
    explicit SystemRowDataSource(SystemsListTab* owner)
        : owner_(owner)
    {
    }

    int numberOfSections(brls::RecyclerFrame*) override { return 1; }
    int numberOfRows(brls::RecyclerFrame*, int) override
    {
        return static_cast<int>(owner_->rowCount());
    }
    float heightForRow(brls::RecyclerFrame*, brls::IndexPath) override { return kRowHeight; }
    float heightForHeader(brls::RecyclerFrame*, int) override { return 0.f; }
    std::string titleForHeader(brls::RecyclerFrame*, int) override { return {}; }

    brls::RecyclerCell* cellForHeader(brls::RecyclerFrame* recycler, int) override
    {
        auto* cell = recycler->dequeueReusableCell(kEmptyHeaderReuseId);
        cell->setHeight(0.f);
        cell->setVisibility(brls::Visibility::GONE);
        cell->setFocusable(false);
        return cell;
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        return owner_->cellForRow(recycler, static_cast<size_t>(index.row));
    }

    void didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) override
    {
        owner_->openSystemAt(static_cast<size_t>(index.row));
    }

private:
    SystemsListTab* owner_;
};

} // namespace

brls::RecyclerCell* SystemsListTab::cellForRow(brls::RecyclerFrame* recycler, size_t index)
{
    auto* cell = static_cast<SystemRowCell*>(recycler->dequeueReusableCell(kRowReuseId));
    if (index >= systemIds_.size())
        return cell;

    auto& theme = ThemeManager::instance();
    const std::string& id = systemIds_[index];
    cell->bind(this, browser::systemDisplayName(id), browser::systemGameCountLabel(id),
               theme.color("brls/text"), theme.color("nxstation/detail_text"));
    return cell;
}

void SystemsListTab::onRowFocused(int row)
{
    if (jumping_ || row < 0 || static_cast<size_t>(row) >= systemIds_.size())
        return;
    const size_t next = static_cast<size_t>(row);
    if (next == index_)
        return;
    int direction = navDir_;
    navDir_ = 0;
    if (direction == 0)
        direction = next > index_ ? 1 : -1;
    playNavSfx();
    showSystem(next, direction);
}

SystemsListTab::SystemsListTab()
{
    inflateFromXMLRes("xml/views/systems_list.xml");

    titleLabel->setText("Systems");
    hintsLabel->setText("↑ ↓ Browse   ·   A Open   ·   Y Menu   ·   + Settings");

    artImage->setScalingType(brls::ImageScalingType::FIT);
    artImage->setImageAlign(brls::ImageAlignment::CENTER);
    artImageNext->setScalingType(brls::ImageScalingType::FIT);
    artImageNext->setImageAlign(brls::ImageAlignment::CENTER);
    artImageNext->setVisibility(brls::Visibility::GONE);
    artFrame->setClipsToBounds(true);
    previewCard->setClipsToBounds(true);
    systemNameLabel->setSingleLine(true);
    systemNameLabel->setAnimated(true);
    systemNameLabel->setFontSize(28.f);
    detailLabel->setLineHeight(1.45f);
    detailLabel->setSingleLine(false);
    detailLabel->setFontSize(19.f); // ~14pt
    detailScroller->setScrollingIndicatorVisible(false);

    scroller->estimatedRowHeight = kRowHeight;
    scroller->setScrollingIndicatorVisible(Config::instance().romListScrollbar());
    scroller->registerCell(kRowReuseId, []() { return new SystemRowCell(); });
    scroller->registerCell(kEmptyHeaderReuseId, []() {
        auto* cell = new brls::RecyclerCell();
        cell->setHeight(0.f);
        cell->setVisibility(brls::Visibility::GONE);
        cell->setFocusable(false);
        return cell;
    });
    scroller->setDataSource(new SystemRowDataSource(this), true);

    auto consume = [](brls::View*) { return true; };
    scroller->registerAction("", brls::ControllerButton::BUTTON_NAV_UP, consume, true, true);
    scroller->registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, consume, true, true);
    scroller->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, consume, true, true);
    scroller->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, consume, true, true);

    rebuildSystemIds();
    registerActions();
    applyThemeColors();

    if (!systemIds_.empty()) {
        index_ = browser::initialSystemIndex(systemIds_);
        reloadList();
        showSystem(index_, 0);
    } else {
        countLabel->setText("0 systems");
        systemNameLabel->setText("No Systems");
        detailLabel->setText("No systems to show.");
        browser::applySystemListArt(artImage, {});
    }
}

void SystemsListTab::registerActions()
{
    auto consume = [](brls::View*) { return true; };
    registerAction("", brls::ControllerButton::BUTTON_NAV_UP, consume, true, true);
    registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, consume, true, true);
    registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, consume, true, true);
    registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, consume, true, true);

    registerAction("Open", brls::ControllerButton::BUTTON_A, [this](brls::View*) {
        openSystemAt(index_);
        return true;
    });
    registerAction(
        "Settings", brls::ControllerButton::BUTTON_START, [](brls::View*) {
            SF_LOG_ACTION("Systems/Settings");
            playConfirmSfx();
            brls::sync([]() { SettingsTab::open(); });
            return true;
        });
    registerAction("Menu", brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        SF_LOG_ACTION("Systems/MainMenu");
        playConfirmSfx();
        MainMenuOptionsView::present([this](MainMenuAction action) {
            if (action == MainMenuAction::Search)
                GameSearch::prompt();
            else if (action == MainMenuAction::JumpToSystem)
                SystemJumpView::present(systemIds_, [this](std::string systemId) {
                    for (size_t i = 0; i < systemIds_.size(); ++i) {
                        if (systemIds_[i] == systemId) {
                            scroller->selectRowAt(brls::IndexPath(0, i), false);
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
}

void SystemsListTab::rebuildSystemIds()
{
    browser::rebuildSystemIds(systemIds_);
    if (index_ >= systemIds_.size())
        index_ = systemIds_.empty() ? 0 : systemIds_.size() - 1;
}

void SystemsListTab::reloadList()
{
    countLabel->setText(std::to_string(systemIds_.size()) +
                        (systemIds_.size() == 1 ? " system" : " systems"));
    scroller->reloadData();
    if (!systemIds_.empty()) {
        const size_t row = std::min(index_, systemIds_.size() - 1);
        scroller->selectRowAt(brls::IndexPath(0, row), false);
        brls::Application::giveFocus(scroller);
    }
}

void SystemsListTab::applyThemeColors()
{
    auto& theme = ThemeManager::instance();
    titleLabel->setTextColor(theme.color("brls/text"));
    countLabel->setTextColor(theme.color("nxstation/count_text"));
    hintsLabel->setTextColor(theme.color("nxstation/hint_text"));
    systemNameLabel->setTextColor(theme.color("brls/text"));
    detailLabel->setTextColor(theme.color("nxstation/preview_text"));
    previewCard->setBackgroundColor(theme.color("nxstation/card_bg"));
    setBackgroundColor(theme.color("brls/background"));
    scroller->reloadData();
    if (!systemIds_.empty()) {
        scroller->selectRowAt(brls::IndexPath(0, index_), false);
        brls::Application::giveFocus(scroller);
    }
    invalidate();
}

void SystemsListTab::resetArtTransform()
{
    artImage->setTranslationX(0.f);
    artImageNext->setTranslationX(0.f);
    artImage->setAlpha(1.f);
    artImageNext->setAlpha(1.f);
    setImageZoom(artImage, 1.f);
    setImageZoom(artImageNext, 1.f);
    artImageNext->setVisibility(brls::Visibility::GONE);
}

void SystemsListTab::cancelPreviewTransition()
{
    fadeAnim_.stop();
    slideAnim_.stop();
    transitioning_ = false;
    resetArtTransform();
    artImage->setAlpha(1.f);
    detailLabel->setAlpha(1.f);
}

void SystemsListTab::applyPreview(size_t index)
{
    if (systemIds_.empty() || index >= systemIds_.size()) {
        systemNameLabel->setText("No Systems");
        detailLabel->setText("No systems to show.");
        browser::applySystemListArt(artImage, {});
        resetArtTransform();
        return;
    }

    const std::string& id = systemIds_[index];
    browser::applySystemListArt(artImage, id);
    systemNameLabel->setText(browser::systemDisplayName(id));
    detailLabel->setText(browser::systemDescription(id));
    detailScroller->setContentOffsetY(0.f, false);
    resetArtTransform();
}

void SystemsListTab::showSystem(size_t index, int direction)
{
    if (systemIds_.empty()) {
        countLabel->setText("0 systems");
        applyPreview(0);
        return;
    }

    index = index % systemIds_.size();
    if (index == index_ && direction == 0) {
        applyPreview(index);
        return;
    }

    beginPreviewTransition(index, direction);
}

void SystemsListTab::applyLightPreview(size_t index)
{
    if (systemIds_.empty() || index >= systemIds_.size())
        return;
    index_ = index;
    const std::string& id = systemIds_[index];
    systemNameLabel->setText(browser::systemDisplayName(id));
    detailLabel->setText(browser::systemDescription(id));
}

void SystemsListTab::stepBy(int delta, bool repeating)
{
    if (systemIds_.empty() || delta == 0)
        return;
    const int n = static_cast<int>(systemIds_.size());
    int next = static_cast<int>(index_) + delta;
    next %= n;
    if (next < 0)
        next += n;
    if (static_cast<size_t>(next) == index_)
        return;

    navDir_ = delta > 0 ? 1 : -1;
    const bool wrap = std::abs(next - static_cast<int>(index_)) != 1;
    jumping_ = true;
    index_ = static_cast<size_t>(next);
    scroller->selectRowAt(brls::IndexPath(0, index_), !wrap);
    brls::Application::giveFocus(scroller);
    jumping_ = false;

    if (!repeating)
        playNavSfx();

    if (repeating) {
        cancelPreviewTransition();
        applyLightPreview(index_);
        navPreviewDeferred_ = true;
        return;
    }

    navPreviewDeferred_ = false;
    showSystem(index_, navDir_);
}

void SystemsListTab::openSystemAt(size_t index)
{
    if (systemIds_.empty() || index >= systemIds_.size())
        return;
    playConfirmSfx();
    const std::string systemId = systemIds_[index];
    SF_LOG_ACTION("Systems/Open");
    NavigationState::setLastSystem(systemId);
    brls::sync([systemId]() { PushedActivity::push(new GameListView(systemId)); });
}

void SystemsListTab::beginPreviewTransition(size_t nextIndex, int direction)
{
    cancelPreviewTransition();
    switch (Config::instance().carouselTransition()) {
    case CarouselTransition::Slide:
        beginPreviewSlide(nextIndex, direction);
        break;
    case CarouselTransition::Crossfade:
        beginPreviewCrossfade(nextIndex);
        break;
    case CarouselTransition::Zoom:
        beginPreviewZoom(nextIndex);
        break;
    case CarouselTransition::None:
        index_ = nextIndex % systemIds_.size();
        applyPreview(index_);
        break;
    case CarouselTransition::Fade:
    default:
        beginPreviewFade(nextIndex);
        break;
    }
}

void SystemsListTab::beginPreviewSlide(size_t nextIndex, int direction)
{
    transitioning_ = true;
    const float width = artFrame->getWidth() > 1.f ? artFrame->getWidth() : 480.f;
    browser::applySystemListArt(artImageNext, systemIds_[nextIndex]);
    artImageNext->setVisibility(brls::Visibility::VISIBLE);
    artImageNext->setAlpha(1.f);
    setImageZoom(artImage, 1.f);
    setImageZoom(artImageNext, 1.f);
    artImage->setTranslationX(0.f);
    artImageNext->setTranslationX(direction * width);

    slideAnim_.stop();
    slideAnim_.reset(0.f);
    slideAnim_.addStep(1.f, kTransitionMs, brls::EasingFunction::quadraticOut);
    slideAnim_.setTickCallback([this, width, direction]() {
        const float t = slideAnim_.getValue();
        artImage->setTranslationX(-direction * width * t);
        artImageNext->setTranslationX(direction * width * (1.f - t));
        invalidate();
    });
    slideAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        index_ = nextIndex % systemIds_.size();
        applyPreview(index_);
        transitioning_ = false;
        invalidate();
    });
    slideAnim_.start();
}

void SystemsListTab::beginPreviewFade(size_t nextIndex)
{
    transitioning_ = true;
    constexpr int kFadeMs = 140;
    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kFadeMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        const float a = 1.f - fadeAnim_.getValue();
        artImage->setAlpha(a);
        detailLabel->setAlpha(0.65f + a * 0.35f);
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        index_ = nextIndex % systemIds_.size();
        applyPreview(index_);
        artImage->setAlpha(0.f);
        fadeAnim_.reset(1.f);
        fadeAnim_.addStep(0.f, kFadeMs, brls::EasingFunction::quadraticIn);
        fadeAnim_.setTickCallback([this]() {
            const float a = 1.f - fadeAnim_.getValue();
            artImage->setAlpha(a);
            detailLabel->setAlpha(0.65f + a * 0.35f);
        });
        fadeAnim_.setEndCallback([this](bool) {
            artImage->setAlpha(1.f);
            detailLabel->setAlpha(1.f);
            transitioning_ = false;
        });
        fadeAnim_.start();
    });
    fadeAnim_.start();
}

void SystemsListTab::beginPreviewCrossfade(size_t nextIndex)
{
    transitioning_ = true;
    browser::applySystemListArt(artImageNext, systemIds_[nextIndex]);
    artImageNext->setVisibility(brls::Visibility::VISIBLE);
    artImageNext->setTranslationX(0.f);
    artImage->setTranslationX(0.f);
    artImageNext->setAlpha(0.f);
    setImageZoom(artImage, 1.f);
    setImageZoom(artImageNext, 1.f);

    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kTransitionMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        const float t = fadeAnim_.getValue();
        artImageNext->setAlpha(t);
        artImage->setAlpha(1.f - t);
        invalidate();
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        index_ = nextIndex % systemIds_.size();
        applyPreview(index_);
        transitioning_ = false;
        invalidate();
    });
    fadeAnim_.start();
}

void SystemsListTab::beginPreviewZoom(size_t nextIndex)
{
    transitioning_ = true;
    browser::applySystemListArt(artImageNext, systemIds_[nextIndex]);
    artImageNext->setVisibility(brls::Visibility::VISIBLE);
    artImageNext->setTranslationX(0.f);
    artImage->setTranslationX(0.f);
    artImageNext->setAlpha(0.f);
    setImageZoom(artImage, 1.f);
    setImageZoom(artImageNext, 1.12f);

    fadeAnim_.stop();
    fadeAnim_.reset(0.f);
    fadeAnim_.addStep(1.f, kTransitionMs, brls::EasingFunction::quadraticOut);
    fadeAnim_.setTickCallback([this]() {
        const float t = fadeAnim_.getValue();
        artImageNext->setAlpha(t);
        artImage->setAlpha(1.f - t);
        setImageZoom(artImageNext, 1.12f - 0.12f * t);
        setImageZoom(artImage, 1.f + 0.06f * t);
        invalidate();
    });
    fadeAnim_.setEndCallback([this, nextIndex](bool finished) {
        if (!finished) {
            transitioning_ = false;
            return;
        }
        index_ = nextIndex % systemIds_.size();
        applyPreview(index_);
        transitioning_ = false;
        invalidate();
    });
    fadeAnim_.start();
}

void SystemsListTab::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    idleTimer_.reset();
    scroller->setScrollingIndicatorVisible(Config::instance().romListScrollbar());
    applyThemeColors();
    rebuildSystemIds();
    reloadList();
    if (!systemIds_.empty())
        showSystem(index_, 0);
    brls::Application::giveFocus(scroller);
}

void SystemsListTab::tickButtonNavigation(float delta)
{
    if (systemIds_.empty())
        return;

    const auto& pad = brls::Application::getControllerState();
    const bool up = pad.buttons[brls::BUTTON_NAV_UP];
    const bool down = pad.buttons[brls::BUTTON_NAV_DOWN];

    pollHold(dpadHoldDir_, dpadHoldTime_, dpadRepeatTimer_, dpadRepeatCount_, delta, up, down,
             [this](int dir, bool repeating) { stepBy(dir, repeating); });

    if (dpadHoldDir_ == 0 && navPreviewDeferred_) {
        navPreviewDeferred_ = false;
        showSystem(index_, 0);
    }
}

void SystemsListTab::tickIdleScreensaver()
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

void SystemsListTab::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        applyThemeColors();
        if (!systemIds_.empty())
            applyPreview(index_);
    }

    if (browser::consumeSystemsDataRefresh()) {
        applyThemeColors();
        rebuildSystemIds();
        reloadList();
        if (!systemIds_.empty())
            showSystem(index_, 0);
    }

    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    tickIdleScreensaver();
    // D-pad and left-stick share BUTTON_NAV_* — don't also step from analog axes.
    tickButtonNavigation(delta);
    brls::Box::frame(ctx);
}

} // namespace sf::ui
