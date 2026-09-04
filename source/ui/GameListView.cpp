#include "ui/GameListView.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "app/ScreenScraperCredentials.hpp"
#include "media/ManualPages.hpp"
#include "media/TextureCache.hpp"
#include "media/VideoPlayer.hpp"
#include "scraper/GamelistXml.hpp"
#include "scraper/ScrapeTypes.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "ui/GameDetailView.hpp"
#include "ui/GameOptionsMenuView.hpp"
#include "ui/LibraryScanView.hpp"
#include "ui/GameSearch.hpp"
#include "ui/AlphabetJumpView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ScrapeMenuView.hpp"
#include "ui/ScrapeProgressView.hpp"
#include "ui/ScreensaverView.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "ui/ZoomImage.hpp"
#include "ui/UiTransition.hpp"
#include "ui/VideoPreviewView.hpp"
#include "util/Favorites.hpp"
#include "util/GameArt.hpp"
#include "util/FileSystem.hpp"
#include "util/ActionLog.hpp"
#include "util/Logger.hpp"
#include "util/NavigationState.hpp"
#include "util/Placeholders.hpp"
#include "ui/LaunchTransition.hpp"
#include "util/LastPlayed.hpp"
#include "util/VirtualSystems.hpp"
#include "util/IdleTimer.hpp"
#include "util/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <nanovg.h>
#include <random>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf::ui {

namespace {

std::string systemLabelFor(const std::string& systemId)
{
    if (isVirtualSystemId(systemId))
        return virtualSystemDisplayName(systemId);
    if (const SystemConfig* sys = Config::instance().findSystem(systemId))
        return sys->name;
    return systemId;
}

constexpr const char* kPlaceholderDescription =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam.";

constexpr float kRowHeight = 64.f;
constexpr const char* kRowReuseId = "GameRow";
constexpr const char* kEmptyHeaderReuseId = "EmptyHeader";

// Description auto-scroll: dwell at each end, then creep down at a readable pace.
constexpr float kDescriptionHoldTopSeconds = 2.5f;
constexpr float kDescriptionHoldBottomSeconds = 3.0f;
constexpr float kDescriptionPixelsPerSecond = 28.f;

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

void pollAcceleratedHold(int& holdDir, float& holdTime, float& repeatTimer, int& repeatCount, float delta,
                         int negative, int positive,
                         const std::function<void(int dir, bool repeating)>& step)
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

void applyGameArtPlaceholder(brls::Image* image)
{
    if (std::string custom = gameArtPlaceholderPath(); !custom.empty())
        image->setImageFromFile(custom);
    else
        image->setImageFromRes(gameArtPlaceholderRes());
}

std::string gameSystemId(const sf::GameItem& game, const std::string& listSystemId)
{
    return game.systemId.empty() ? listSystemId : game.systemId;
}

/** A single game row. Only the rows currently on screen exist as real views —
 *  the RecyclerFrame recycles them as the list scrolls, so opening even a
 *  900-game library only ever builds ~15 row views. */
class GameRowCell : public brls::RecyclerCell {
public:
    GameRowCell()
    {
        this->setAxis(brls::Axis::ROW);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setPaddingLeft(4);
        this->setPaddingRight(8);
        this->setHeight(kRowHeight);
        this->setFocusable(true);
        this->setFocusSound(brls::SOUND_NONE);

        title_ = new brls::Label();
        title_->setGrow(1.f);
        title_->setSingleLine(true);
        title_->setAnimated(false);
        title_->setFontSize(20);
        this->addView(title_);

        detail_ = new brls::Label();
        detail_->setSingleLine(true);
        detail_->setAnimated(false);
        detail_->setFontSize(15);
        detail_->setMarginLeft(12);
        detail_->setTextColor(ThemeManager::instance().color("nxstation/detail_text"));
        this->addView(detail_);
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
        if (owner_ && owner_->deferGameRowHighlight()) {
            brls::sync([this]() {
                if (!owner_)
                    return;
                owner_->finishDeferredRowHighlight();
            });
        } else {
            setHideHighlight(false);
        }
        applyFocusTextColor(true);
        if (owner_) {
            owner_->trackFocusedRow(this);
            owner_->onRowFocused(static_cast<size_t>(getIndexPath().row));
        }
    }

    void onFocusLost() override
    {
        brls::RecyclerCell::onFocusLost();
        applyRowSeparator();
        applyFocusTextColor(false);
    }

    void prepareForReuse() override
    {
        alpha.stop();
        setAlpha(1.f);
        // Hide highlight before this cell is moved to a new slot. Otherwise a focused
        // row recycled off the top (fast page-down) briefly paints its selection
        // rectangle behind the system title.
        this->setHideHighlight(true);
        if (brls::Application::getCurrentFocus() == this) {
            if (brls::View* parent = this->getParent()) {
                if (brls::View* frame = parent->getParent())
                    brls::Application::giveFocus(frame);
            }
        }
        owner_ = nullptr;
        applyRowSeparator();
    }

    void bind(GameListView* owner, const std::string& title, const std::string& detail,
              bool showDetail, NVGcolor color)
    {
        title_->setText(title);
        idleTextColor_ = color;
        title_->setTextColor(color);
        detail_->setVisibility(showDetail ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (showDetail)
            detail_->setText(detail);
        applyRowSeparator();
        // Assign owner last so onFocusGained() firing during layout doesn't race a stale index.
        owner_ = owner;
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
        title_->setTextColor(focused ? ThemeManager::instance().selectionTextColor(idleTextColor_)
                                     : idleTextColor_);
    }

    GameListView* owner_ = nullptr;
    brls::Label* title_ = nullptr;
    brls::Label* detail_ = nullptr;
    NVGcolor idleTextColor_{0, 0, 0, 0};
};

class GameRowDataSource : public brls::RecyclerDataSource {
public:
    explicit GameRowDataSource(GameListView* owner)
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

    // Borealis always inserts a section header slot. The default RecyclerHeader still
    // measures to a non-zero height even when the title is empty, which leaves a blank
    // "ghost row" gap under the system name. Return a truly empty cell instead.
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
        owner_->onRowClicked(static_cast<size_t>(index.row));
    }

private:
    GameListView* owner_;
};

} // namespace

namespace {

void setupGameListScroller(brls::RecyclerFrame* scroller, GameListView* owner)
{
    scroller->estimatedRowHeight = kRowHeight;
    scroller->setScrollingIndicatorVisible(Config::instance().romListScrollbar());
    scroller->registerCell(kRowReuseId, []() { return new GameRowCell(); });
    scroller->registerCell(kEmptyHeaderReuseId, []() {
        auto* cell = new brls::RecyclerCell();
        cell->setHeight(0.f);
        cell->setVisibility(brls::Visibility::GONE);
        cell->setFocusable(false);
        return cell;
    });
    scroller->setDataSource(new GameRowDataSource(owner), true);
}

} // namespace

GameListView::GameListView(std::string title, std::vector<sf::GameItem> games,
                           std::string subtitle)
    : games_(std::move(games))
    , customList_(true)
    , customTitle_(std::move(title))
    , customSubtitle_(std::move(subtitle))
{
    this->inflateFromXMLRes("xml/views/game_list.xml");

    titleLabel->setText(customTitle_);

    boxArt->setScalingType(brls::ImageScalingType::FIT);
    boxArt->setImageAlign(brls::ImageAlignment::CENTER);
    artFrame->setClipsToBounds(true);
    previewCard->setClipsToBounds(true);
    applyGameArtPlaceholder(boxArt);
    detailLabel->setText(kPlaceholderDescription);
    detailLabel->setLineHeight(1.35f);
    detailLabel->setSingleLine(false);
    detailScroller->setScrollingIndicatorVisible(false);
    hintsLabel->setText("A Launch   ·   X Star   ·   Y Menu   ·   R₃ Random   ·   L/R Page");
    applyThemeColors();

    setupGameListScroller(scroller, this);

    installVideoPreview();
    countLabel->setText(customSubtitle_);

    registerListActions();
}

GameListView::GameListView(std::string systemId, size_t initialFocus)
    : systemId_(std::move(systemId))
    , initialFocus_(initialFocus)
{
    this->inflateFromXMLRes("xml/views/game_list.xml");

    const SystemConfig* sys = Config::instance().findSystem(systemId_);
    if (isVirtualSystemId(systemId_))
        titleLabel->setText(virtualSystemDisplayName(systemId_));
    else
        titleLabel->setText(sys ? sys->name : systemId_);

    boxArt->setScalingType(brls::ImageScalingType::FIT);
    boxArt->setImageAlign(brls::ImageAlignment::CENTER);
    artFrame->setClipsToBounds(true);
    previewCard->setClipsToBounds(true);
    applyGameArtPlaceholder(boxArt);
    detailLabel->setText(kPlaceholderDescription);
    detailLabel->setLineHeight(1.35f);
    detailLabel->setSingleLine(false);
    detailScroller->setScrollingIndicatorVisible(false);
    hintsLabel->setText("A Launch   ·   X Star   ·   Y Menu   ·   R₃ Random   ·   L/R Page");
    applyThemeColors();

    setupGameListScroller(scroller, this);

    installVideoPreview();
    countLabel->setText("Loading...");

    registerListActions();
}

void GameListView::registerListActions()
{
    auto alive = alive_;
    this->registerAction(
        "Menu", brls::ControllerButton::BUTTON_Y, [this, alive](brls::View*) {
        if (!*alive || overlayBlocking_)
            return true;
        playConfirmSfx();
        openOptionsMenu();
        return true;
    });

    this->registerAction(
        "Star", brls::ControllerButton::BUTTON_X, [this, alive](brls::View*) {
        if (!*alive || overlayBlocking_ || focusedIndex_ >= games_.size())
            return true;
        playToggleSfx();
        toggleFavorite(focusedIndex_);
        return true;
    });

    // Consume D-pad up/down so we can drive accelerated list scrolling from frame().
    auto consumeNav = [](brls::View*) { return true; };
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_UP, consumeNav, true, true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, consumeNav, true, true);

    this->registerAction(
        "Random", brls::ControllerButton::BUTTON_RSB, [this, alive](brls::View*) {
        if (!*alive || overlayBlocking_ || games_.empty())
            return true;
        playNavSfx();
        jumpToRandomGame();
        return true;
    });
}

GameListView::~GameListView()
{
    *alive_ = false;
    previewCard->alpha.stop();
    AppState::instance().video().stop();
    SF_LOG_D("UI", "GameListView destroyed: %s", systemId_.c_str());
}

void GameListView::installVideoPreview()
{
    videoPreview_ = new VideoPreviewView();
    videoPreview_->setVisibility(brls::Visibility::GONE);
    artFrame->addView(videoPreview_);
}

void GameListView::updatePreviewSurface(brls::FrameContext* ctx)
{
    if (!videoPreview_)
        return;

    videoPreview_->pump(ctx);

    const bool showVideo = videoPreview_->showingVideo();
    if (showVideo == videoVisible_)
        return;

    videoVisible_ = showVideo;
    videoPreview_->setVisibility(showVideo ? brls::Visibility::VISIBLE
                                           : brls::Visibility::GONE);
    boxArt->setVisibility(showVideo ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

void GameListView::resetDescriptionScroll()
{
    descriptionPhase_ = DescriptionPhase::HoldTop;
    descriptionOffset_ = 0.f;
    descriptionHold_ = 0.f;
    manualDescriptionScroll_ = false;
    detailScroller->setContentOffsetY(0.f, false);
}

void GameListView::tickDescriptionScroll(float delta)
{
    if (manualDescriptionScroll_)
        return;

    const float viewHeight = detailScroller->getHeight();
    const float contentHeight = detailLabel->getHeight();
    const float maxOffset = contentHeight - viewHeight;

    if (maxOffset <= 1.f) {
        if (descriptionOffset_ != 0.f)
            resetDescriptionScroll();
        return;
    }

    switch (descriptionPhase_) {
        case DescriptionPhase::HoldTop:
            descriptionHold_ += delta;
            if (descriptionHold_ >= kDescriptionHoldTopSeconds) {
                descriptionHold_ = 0.f;
                descriptionPhase_ = DescriptionPhase::Scrolling;
            }
            return;

        case DescriptionPhase::Scrolling:
            descriptionOffset_ += kDescriptionPixelsPerSecond * delta;
            if (descriptionOffset_ >= maxOffset) {
                descriptionOffset_ = maxOffset;
                descriptionPhase_ = DescriptionPhase::HoldBottom;
            }
            break;

        case DescriptionPhase::HoldBottom:
            descriptionHold_ += delta;
            if (descriptionHold_ >= kDescriptionHoldBottomSeconds) {
                descriptionHold_ = 0.f;
                descriptionOffset_ = 0.f;
                descriptionPhase_ = DescriptionPhase::HoldTop;
                detailScroller->setContentOffsetY(0.f, false);
            }
            return;
    }

    // Only push whole-pixel changes: every offset write triggers a layout pass.
    const float applied = std::floor(descriptionOffset_);
    if (applied != std::floor(detailScroller->getContentOffsetY()))
        detailScroller->setContentOffsetY(applied, false);
}

void GameListView::tickDescriptionStickScroll(float delta)
{
    if (!Config::instance().rightStickDescriptionScroll())
        return;

    const auto& pad = brls::Application::getControllerState();
    const float stickY = pad.axes[brls::RIGHT_Y];
    constexpr float kDeadzone = 0.2f;
    if (std::abs(stickY) <= kDeadzone)
        return;

    manualDescriptionScroll_ = true;

    const float viewHeight = detailScroller->getHeight();
    const float contentHeight = detailLabel->getHeight();
    const float maxOffset = std::max(0.f, contentHeight - viewHeight);
    if (maxOffset <= 1.f)
        return;

    float offset = detailScroller->getContentOffsetY();
    offset += stickY * 360.f * delta;
    const float clamped = std::clamp(offset, 0.f, maxOffset);
    detailScroller->setContentOffsetY(clamped, false);
    descriptionOffset_ = clamped;
    descriptionPhase_ = DescriptionPhase::HoldTop;
    descriptionHold_ = 0.f;
}

void GameListView::applyLightPreview(size_t index)
{
    if (index >= games_.size())
        return;

    focusedIndex_ = index;
    const auto& g = games_[index];
    detailLabel->setText(g.meta.description.empty() ? kPlaceholderDescription : g.meta.description);
}

void GameListView::stepListNav(int direction, bool repeating)
{
    if (overlayBlocking_ || games_.empty())
        return;

    size_t next = focusedIndex_;
    if (direction < 0) {
        if (next == 0)
            return;
        --next;
    } else {
        if (next + 1 >= games_.size())
            return;
        ++next;
    }
    if (next == focusedIndex_)
        return;

    const int dirSign = direction < 0 ? -1 : 1;
    jumping_ = true;
    scroller->selectRowAt(brls::IndexPath(0, next), true);
    brls::Application::giveFocus(scroller);
    jumping_ = false;
    if (!repeating)
        playNavSfx();

    focusedIndex_ = next;
    if (repeating) {
        applyLightPreview(next);
        navPreviewDeferred_ = true;
        return;
    }

    navPreviewDeferred_ = false;
    onGameFocused(next, dirSign);
}

void GameListView::stepPageNav(int direction, bool repeating)
{
    (void)repeating;
    if (games_.empty() || overlayBlocking_)
        return;

    constexpr size_t kStep = 8;
    size_t next = focusedIndex_;
    if (direction < 0)
        next = (focusedIndex_ >= kStep) ? focusedIndex_ - kStep : 0;
    else
        next = std::min(games_.size() - 1, focusedIndex_ + kStep);

    if (next == focusedIndex_)
        return;

    playNavSfx();
    jumpToIndex(next, false);
}

void GameListView::tickAcceleratedNavigation(float delta)
{
    if (overlayBlocking_ || games_.empty())
        return;

    const auto& pad = brls::Application::getControllerState();
    const bool up = pad.buttons[brls::BUTTON_NAV_UP];
    const bool down = pad.buttons[brls::BUTTON_NAV_DOWN];
    const bool lb = pad.buttons[brls::BUTTON_LB];
    const bool rb = pad.buttons[brls::BUTTON_RB];

    pollAcceleratedHold(navHoldDir_, navHoldTime_, navRepeatTimer_, navRepeatCount_, delta, up, down,
                        [this](int dir, bool repeating) { stepListNav(dir, repeating); });

    pollAcceleratedHold(pageHoldDir_, pageHoldTime_, pageRepeatTimer_, pageRepeatCount_, delta, lb, rb,
                        [this](int dir, bool repeating) { stepPageNav(dir, repeating); });

    if (navHoldDir_ == 0 && navPreviewDeferred_) {
        navPreviewDeferred_ = false;
        onGameFocused(focusedIndex_, 0);
    }
}

void GameListView::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        applyThemeColors();
        if (!games_.empty())
            scroller->reloadData();
    }

    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f; // first frame, or the app was stalled (scan, handoff)

    tickIdleScreensaver();

    // The screensaver draws on top and drives the same shared player; pumping here too
    // would steal its decoded frames.
    if (!ScreensaverView::isShowing()) {
        updatePreviewSurface(ctx);
        if (!overlayBlocking_ && delta > 0.f)
            tickAcceleratedNavigation(delta);
        if (delta > 0.f) {
            tickDescriptionStickScroll(delta);
            tickDescriptionScroll(delta);
        }
    }

    brls::Box::frame(ctx);
}

void GameListView::refreshFromStore()
{
    AppState::instance().video().stop();

    if (!customList_) {
        if (!isVirtualSystemId(systemId_) && !AppState::instance().isSystemScanned(systemId_)) {
            if (Config::instance().libraryScanCompleted()) {
                countLabel->setText("Loading…");
                games_.clear();
                scroller->reloadData();
                const std::string sysId = systemId_;
                auto alive = alive_;
                AppState::instance().pool().enqueue([this, alive, sysId]() {
                    AppState::instance().scanSystem(sysId, false);
                    brls::sync([this, alive]() {
                        if (*alive)
                            refreshFromStore();
                    });
                });
                return;
            }
            countLabel->setText("Not scanned — Y Menu → Scan Games");
            games_.clear();
            scroller->reloadData();
            return;
        }

        games_ = AppState::instance().gamesFor(systemId_);
        // Last Played is already most-recent-first from AppState — do not A–Z sort.
        if (systemId_ != kLastPlayedSystemId) {
            std::stable_sort(games_.begin(), games_.end(),
                             [this](const GameItem& a, const GameItem& b) {
                                 if (!isVirtualSystemId(systemId_)) {
                                     const bool af =
                                         Favorites::instance().isFavorite(systemId_, a.path);
                                     const bool bf =
                                         Favorites::instance().isFavorite(systemId_, b.path);
                                     if (af != bf)
                                         return af > bf;
                                 }
                                 return Utf8::compareTitles(a.displayName, b.displayName);
                             });
        }
        countLabel->setText(std::to_string(games_.size()) + " titles");
    } else {
        countLabel->setText(customSubtitle_);
    }

    size_t keepFocus = focusedIndex_;
    if (initialFocus_ < games_.size())
        keepFocus = initialFocus_;
    if (keepFocus >= games_.size())
        keepFocus = games_.empty() ? 0 : games_.size() - 1;

    if (!games_.empty())
        initialFocus_ = keepFocus;

    rebuildList();
}

void GameListView::syncGamesFromStore()
{
    if (customList_ || isVirtualSystemId(systemId_))
        return;
    games_ = AppState::instance().gamesFor(systemId_);
    // Must match refreshFromStore() ordering. Using the unsorted AppState vector left
    // focusedIndex_ pointing at the wrong title (favorites / A–Z vs scan order) — that
    // made "Scrape Individual Game" hit the next ROM in the store list.
    if (systemId_ != kLastPlayedSystemId) {
        std::stable_sort(games_.begin(), games_.end(),
                         [this](const GameItem& a, const GameItem& b) {
                             if (!isVirtualSystemId(systemId_)) {
                                 const bool af =
                                     Favorites::instance().isFavorite(systemId_, a.path);
                                 const bool bf =
                                     Favorites::instance().isFavorite(systemId_, b.path);
                                 if (af != bf)
                                     return af > bf;
                             }
                             return Utf8::compareTitles(a.displayName, b.displayName);
                         });
    }
}

void GameListView::rebuildList()
{
    if (games_.empty()) {
        scroller->reloadData();
        return;
    }

    // reloadData() builds only the rows visible in the viewport around this focus index —
    // opening a 900-game system costs about the same as opening a 20-game one.
    focusedIndex_ = initialFocus_;
    jumping_ = true;
    scroller->setDefaultCellFocus(brls::IndexPath(0, initialFocus_));
    scroller->reloadData();
    brls::Application::giveFocus(scroller);
    jumping_ = false;
    onGameFocused(initialFocus_, 0);
}

brls::RecyclerCell* GameListView::cellForRow(brls::RecyclerFrame* recycler, size_t index)
{
    auto* cell = static_cast<GameRowCell*>(recycler->dequeueReusableCell(kRowReuseId));
    if (index >= games_.size())
        return cell;

    const auto& g = games_[index];
    const std::string favSystemId = gameSystemId(g, systemId_);
    const bool fav = Favorites::instance().isFavorite(favSystemId, g.path);
    const std::string title = fav ? "★ " + g.displayName : g.displayName;
    const NVGcolor color = fav ? ThemeManager::instance().color("nxstation/favorite_text")
                               : ThemeManager::instance().color("brls/text");
    const bool showSystem = customList_ || isVirtualSystemId(systemId_);
    std::string detail;
    if (systemId_ == kLastPlayedSystemId) {
        detail = systemAcronym(g.systemId);
        if (const std::string when = LastPlayed::formatPlayedAt(g.lastPlayedAt); !when.empty()) {
            if (!detail.empty())
                detail += " · ";
            detail += when;
        }
    } else if (showSystem) {
        detail = systemAcronym(g.systemId);
    }
    cell->bind(this, title, detail, showSystem || systemId_ == kLastPlayedSystemId, color);
    return cell;
}

void GameListView::onRowFocused(size_t index)
{
    if (jumping_ || overlayBlocking_ || index >= games_.size())
        return;
    if (index == focusedIndex_)
        return;
    playNavSfx();
    const int direction =
        focusedIndex_ < games_.size() ? (index > focusedIndex_ ? 1 : (index < focusedIndex_ ? -1 : 0))
                                      : 0;
    onGameFocused(index, direction);
}

void GameListView::onRowClicked(size_t index)
{
    if (overlayBlocking_)
        return;
    // Prefer the previewed index — during fast L/R the cell under the highlight can
    // briefly lag behind focusedIndex_/art.
    const size_t launch = focusedIndex_ < games_.size() ? focusedIndex_ : index;
    if (launch >= games_.size())
        return;
    SF_LOG_ACTION("GameList/Launch");
    playConfirmSfx();
    launchGame(launch);
}

void GameListView::applyThemeColors()
{
    auto& theme = ThemeManager::instance();
    titleLabel->setTextColor(theme.color("brls/text"));
    countLabel->setTextColor(theme.color("nxstation/count_text"));
    hintsLabel->setTextColor(theme.color("nxstation/hint_text"));
    detailLabel->setTextColor(theme.color("nxstation/preview_text"));
    previewCard->setBackgroundColor(theme.color("nxstation/card_bg"));
    this->setBackgroundColor(theme.color("brls/background"));
    this->invalidate();
}

void GameListView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    idleTimer_.reset();
    lastFrameTime_ = 0;
    themeGenApplied_ = ThemeManager::instance().uiGeneration();
    applyThemeColors();
    scroller->setScrollingIndicatorVisible(Config::instance().romListScrollbar());
    AppState::instance().video().setHoverDelaySeconds(Config::instance().hoverDelaySeconds());
    resetDescriptionScroll();
    auto alive = alive_;

    if (!customList_ && !isVirtualSystemId(systemId_) &&
        !AppState::instance().isSystemScanned(systemId_)) {
        countLabel->setText("Not scanned — Y Menu → Scan Games");
    }

    brls::sync([this, alive]() {
        if (*alive)
            refreshFromStore();
    });
}

void GameListView::willDisappear(bool resetState)
{
    *alive_ = false;
    previewCard->alpha.stop();
    previewCard->setAlpha(1.f);
    previewCard->setTranslationX(0.f);
    navHoldDir_ = 0;
    navHoldTime_ = 0.f;
    navRepeatTimer_ = 0.f;
    navRepeatCount_ = 0;
    navPreviewDeferred_ = false;
    pageHoldDir_ = 0;
    pageHoldTime_ = 0.f;
    pageRepeatTimer_ = 0.f;
    pageRepeatCount_ = 0;
    AppState::instance().video().stop();
    AppState::instance().textures().retainOnly({});
    if (videoPreview_) {
        videoVisible_ = false;
        videoPreview_->setVisibility(brls::Visibility::GONE);
        boxArt->setVisibility(brls::Visibility::VISIBLE);
    }
    brls::Box::willDisappear(resetState);
}

void GameListView::toggleFavorite(size_t index)
{
    if (index >= games_.size())
        return;

    const std::string path = games_[index].path;
    const std::string favSystemId = gameSystemId(games_[index], systemId_);
    const bool nowFav = !Favorites::instance().isFavorite(favSystemId, path);
    Favorites::instance().toggle(favSystemId, path);
    AppState::instance().rebuildVirtualSections();
    SystemsTab::requestRefreshAfterSettings();

    size_t keep = index;
    refreshFromStore();

    if (nowFav) {
        if (isVirtualSystemId(systemId_))
            keep = index;
        else
            keep = 0;
        brls::Application::notify("Starred");
    } else {
        brls::Application::notify("Unstarred");
    }

    if (keep < games_.size())
        jumpToIndex(keep);
}

void GameListView::openOptionsMenu()
{
    if (focusedIndex_ >= games_.size())
        return;

    SF_LOG_ACTION("GameList/OptionsMenu");
    const sf::GameItem game = games_[focusedIndex_];
    const std::string favSystemId = gameSystemId(game, systemId_);
    auto alive = alive_;
    auto popOverlay = [this, alive]() {
        if (*alive)
            popOverlayBlock();
    };

    pushOverlayBlock();
    GameOptionsMenuView::present(
        favSystemId, game,
        [this, game, favSystemId, alive, popOverlay](GameOptionsAction action) {
            if (!*alive)
                return;
            if (action == GameOptionsAction::GameManual) {
                const SystemConfig* sys = Config::instance().findSystem(favSystemId);
                if (!sys)
                    return;
                const std::string romStem = FileSystem::stemOf(game.path);
                pushOverlayBlock();
                if (!ManualPages::tryOpenManual(
                        sys->path, romStem, game.meta, game.displayName,
                        [this, alive]() {
                            // Keep the list blocked until the fullscreen dialog is fully gone
                            // so the row highlight does not flash under the closing viewer.
                            if (*alive)
                                schedulePopOverlayBlock(8);
                        })) {
                    popOverlayBlock();
                }
                return;
            }
            if (action == GameOptionsAction::Search) {
                // Options dialog already popped the overlay in onDismiss before this
                // callback runs. Do not push again — the keyboard is modal on its own.
                GameSearch::prompt(customList_ ? std::string() : systemId_);
                return;
            }
            if (action == GameOptionsAction::JumpAlphabet) {
                pushOverlayBlock();
                AlphabetJumpView::present(
                    [this, alive](char bucket) {
                        if (!*alive)
                            return;
                        // clearOverlayBlock: letter pick used to call non-virtual Dialog::close,
                        // which skipped FocusedMenuDialog::onDismiss and left the list blocked.
                        clearOverlayBlock();
                        jumpToBucket(bucket);
                    },
                    [this, alive]() {
                        if (*alive)
                            clearOverlayBlock();
                    });
                return;
            }
            if (action == GameOptionsAction::ShowMetadata) {
                const SystemConfig* sys = Config::instance().findSystem(favSystemId);
                if (!sys)
                    return;
                pushOverlayBlock();
                AppState::instance().video().stop();
                GameDetailView::presentMetadata(game, *sys, [this, alive]() {
                    if (!*alive)
                        return;
                    popOverlayBlock();
                    refreshFromStore();
                });
                return;
            }
            if (action == GameOptionsAction::ScanGames) {
                if (!customList_ && !isVirtualSystemId(systemId_)) {
                    pushOverlayBlock();
                    LibraryScanView::presentSystem(systemId_, true, popOverlay);
                } else {
                    brls::Application::notify("Scan is only available for a system library");
                }
                return;
            }
            if (action == GameOptionsAction::Rename) {
                renameFocusedGame();
                return;
            }
            if (action == GameOptionsAction::Delete) {
                confirmDeleteFocusedGame();
                return;
            }
            openScrapeMenu();
        },
        popOverlay);
}

void GameListView::jumpToIndex(size_t index, bool animated)
{
    if (index >= games_.size())
        return;

    // Always jump instantly. Animated selectRowAt keeps focus on the old cell while the
    // list scrolls, then recycles that cell to the top — which is why fast R scrolling
    // flashed the selection rectangle behind the system name and left highlight ~2 rows
    // away from the preview/art.
    (void)animated;
    jumping_ = true;
    focusedIndex_ = index;
    scroller->selectRowAt(brls::IndexPath(0, index), false);
    brls::Application::giveFocus(scroller);
    jumping_ = false;
    onGameFocused(index, 0);
}

void GameListView::jumpToBucket(char bucket)
{
    if (games_.empty())
        return;

    // Starred games are pinned above the A–Z list on normal systems. Skip them so
    // "Jump to letter" lands on the alphabetical section, not a favorite that happens
    // to start with that letter.
    const bool skipStarred = !customList_ && !isVirtualSystemId(systemId_);

    for (size_t i = 0; i < games_.size(); ++i) {
        const auto& g = games_[i];
        if (skipStarred) {
            const std::string favSystemId = gameSystemId(g, systemId_);
            if (Favorites::instance().isFavorite(favSystemId, g.path))
                continue;
        }
        if (Utf8::startsWithBucket(g.displayName, bucket)) {
            jumpToIndex(i, false);
            return;
        }
    }

    brls::Application::notify("No games starting with " + std::string(1, bucket));
}

void GameListView::jumpToRandomGame()
{
    if (games_.empty())
        return;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, games_.size() - 1);
    jumpToIndex(dist(rng), false);
}

void GameListView::renameFocusedGame()
{
    if (focusedIndex_ >= games_.size())
        return;

    const sf::GameItem game = games_[focusedIndex_];
    const std::string systemId = gameSystemId(game, systemId_);
    const std::string ext = FileSystem::extensionOf(game.path);
    const std::string oldStem = FileSystem::stemOf(game.path);

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

    const std::string parent = FileSystem::parentPath(game.path);
    const std::string newPath = FileSystem::join(parent, newStem + ext);
    if (FileSystem::exists(newPath)) {
        brls::Application::notify("A file with that name already exists");
        return;
    }

    if (!FileSystem::renameFile(game.path, newPath)) {
        brls::Application::notify("Rename failed");
        return;
    }

    if (!AppState::instance().renameGame(systemId, game.path, newPath)) {
        brls::Application::notify("Renamed on SD but library update failed — rescan");
        return;
    }

    SystemsTab::requestRefreshAfterSettings();
    brls::Application::notify("Renamed");
    refreshFromStore();
}

void GameListView::confirmDeleteFocusedGame()
{
    if (focusedIndex_ >= games_.size())
        return;

    const sf::GameItem game = games_[focusedIndex_];
    auto alive = alive_;
    pushOverlayBlock();

    auto* panel = new brls::Box();
    stylePopupMenuPanel(panel);

    auto* header = new brls::Header();
    header->setTitle("Delete ROM?");
    header->setSubtitle(game.filename);
    panel->addView(header);

    auto* warn = new brls::Label();
    warn->setText("This permanently deletes the file from the SD card.");
    warn->setFontSize(18);
    warn->setMarginTop(8);
    warn->setMarginBottom(12);
    warn->setTextColor(ThemeManager::instance().color("nxstation/detail_text"));
    panel->addView(warn);

    auto dialogHolder = std::make_shared<FocusedMenuDialog*>(nullptr);

    auto* delRow = new brls::DetailCell();
    delRow->setText("Delete");
    stylePopupMenuRow(delRow);
    delRow->registerClickAction([this, game, alive, dialogHolder](brls::View*) {
        if (*dialogHolder) {
            (*dialogHolder)->close([this, game, alive] {
                if (*alive)
                    deleteGame(game);
            });
        } else if (*alive) {
            deleteGame(game);
        }
        return true;
    });
    panel->addView(delRow);

    auto* cancelRow = new brls::DetailCell();
    cancelRow->setText("Cancel");
    stylePopupMenuRow(cancelRow);
    cancelRow->registerClickAction([dialogHolder](brls::View*) {
        if (*dialogHolder)
            (*dialogHolder)->close();
        return true;
    });
    panel->addView(cancelRow);

    *dialogHolder = FocusedMenuDialog::present(panel, [this, alive]() {
        if (*alive)
            popOverlayBlock();
    });
}

void GameListView::deleteGame(const sf::GameItem& game)
{
    const std::string systemId = gameSystemId(game, systemId_);
    AppState::instance().video().stop();

    if (!FileSystem::removeFile(game.path)) {
        brls::Application::notify("Could not delete file");
        return;
    }

    AppState::instance().removeGame(systemId, game.path);
    SystemsTab::requestRefreshAfterSettings();
    brls::Application::notify("Deleted");
    refreshFromStore();
}

void GameListView::tickIdleScreensaver()
{
    const int idleSec = Config::instance().screensaverIdleSeconds();
    if (idleSec <= 0 || overlayBlocking_ || ScreensaverView::isActive()) {
        idleTimer_.reset();
        return;
    }

    idleTimer_.pollController();
    if (!idleTimer_.idleSeconds(static_cast<float>(idleSec)))
        return;

    idleTimer_.reset();

    auto alive = alive_;
    ScreensaverView::present([this, alive]() {
        if (!*alive)
            return;
        idleTimer_.reset();
        // The screensaver hijacked the shared player; put this row's preview back.
        if (focusedIndex_ < games_.size())
            onGameFocused(focusedIndex_, 0);
    });
}

void GameListView::openScrapeMenu()
{
    if (!Config::instance().hasScreenScraperWebsiteLogin()) {
        brls::Application::notify(
            "ScreenScraper login required — set website ssid/password in Settings");
        return;
    }
    if (!screenscraper::devCredentialsConfigured()) {
        brls::Application::notify("ScreenScraper developer API keys missing in this build");
        return;
    }

    if (focusedIndex_ >= games_.size())
        return;

    // Capture the focused title now — scrape menu steals focus, and a later
    // syncGamesFromStore() used to reshuffle indices vs the on-screen list.
    const sf::GameItem selectedGame = games_[focusedIndex_];
    const std::string scrapeSystemId = gameSystemId(selectedGame, systemId_);
    auto alive = alive_;
    auto popOverlay = [this, alive]() {
        if (*alive)
            popOverlayBlock();
    };

    SF_LOG_ACTION("GameList/ScrapeMenu");
    pushOverlayBlock();
    ScrapeMenuView::present(
        scrapeSystemId,
        [this, scrapeSystemId, selectedGame, alive](ScrapeMode mode) {
            if (!*alive)
                return;
            std::vector<sf::GameItem> snapshot;
            if (mode == ScrapeMode::Single) {
                snapshot.push_back(selectedGame);
            } else {
                syncGamesFromStore();
                snapshot = games_;
            }
            if (snapshot.empty()) {
                brls::Application::notify("No game selected");
                return;
            }
            pushScrapeProgress(mode, std::move(snapshot), scrapeSystemId);
        },
        popOverlay);
}

void GameListView::pushOverlayBlock()
{
    ++overlayDepth_;
    if (overlayDepth_ == 1)
        setOverlayBlocking(true);
}

void GameListView::popOverlayBlock()
{
    if (overlayDepth_ <= 0)
        return;
    --overlayDepth_;
    if (overlayDepth_ == 0)
        setOverlayBlocking(false);
}

void GameListView::schedulePopOverlayBlock(int frames)
{
    if (overlayDepth_ <= 0)
        return;
    if (frames <= 0) {
        popOverlayBlock();
        return;
    }
    brls::sync([this, frames]() { schedulePopOverlayBlock(frames - 1); });
}

void GameListView::finishDeferredRowHighlight()
{
    deferRowHighlight_ = false;
    if (focusedRowView_)
        focusedRowView_->setHideHighlight(false);
}

void GameListView::clearOverlayBlock()
{
    overlayDepth_ = 0;
    if (overlayBlocking_)
        setOverlayBlocking(false);
}

void GameListView::setOverlayBlocking(bool blocked)
{
    if (overlayBlocking_ == blocked)
        return;

    overlayBlocking_ = blocked;
    scroller->setFocusable(!blocked);

    if (blocked) {
        // Dialogs take real focus, but the previously-focused row can still briefly render
        // its highlight during the open transition. Hide it defensively; it's restored by
        // GameRowCell::onFocusGained() the next time this row (or any row) gains focus.
        if (focusedRowView_)
            focusedRowView_->setHideHighlight(true);
        AppState::instance().video().stop();
    } else {
        deferRowHighlight_ = true;
        brls::sync([this]() {
            if (overlayBlocking_)
                return;
            scroller->selectRowAt(brls::IndexPath(0, focusedIndex_), false);
            brls::Application::giveFocus(scroller);
        });
    }
}

void GameListView::pushScrapeProgress(ScrapeMode mode, std::vector<sf::GameItem> games,
                                      const std::string& scrapeSystemId)
{
    pushOverlayBlock();
    auto alive = alive_;
    const std::string systemId = scrapeSystemId.empty() ? systemId_ : scrapeSystemId;
    auto* view = new ScrapeProgressView(systemId, mode, std::move(games));
    view->setOnDismiss([this, alive]() {
        if (*alive) {
            syncGamesFromStore();
            refreshFromStore();
            popOverlayBlock();
        }
    });
    PushedActivity::push(new ScrapeProgressActivity(view));
}

void GameListView::launchGame(size_t index)
{
    if (index >= games_.size())
        return;

    const std::string launchSystemId = gameSystemId(games_[index], systemId_);
    const SystemConfig* sys = Config::instance().findSystem(launchSystemId);
    if (!sys)
        return;

    NavigationState::update(customList_ ? launchSystemId : systemId_, index, games_[index].path);
    NavigationState::setLastSystem(launchSystemId);
    LastPlayed::instance().record(launchSystemId, games_[index].path);
    AppState::instance().rebuildVirtualSections();
    SystemsTab::requestRefreshAfterSettings();

    beginGameLaunch(*sys, games_[index]);
}

void GameListView::applyPreviewContent(size_t index)
{
    if (index >= games_.size())
        return;

    auto& g = games_[index];
    const std::string favSystemId = gameSystemId(g, systemId_);

    if (g.meta.boxArtPath.empty() && g.meta.logoPath.empty() && g.meta.videoPath.empty()) {
        const SystemConfig* sys = Config::instance().findSystem(favSystemId);
        if (sys) {
            GamelistXml::applyFallbackMedia(g.meta, sys->path, g.filename,
                                            FileSystem::stemOf(g.path));
        }
    }

    if (!customList_)
        NavigationState::update(systemId_, index, g.path);

    detailLabel->setText(g.meta.description.empty() ? kPlaceholderDescription : g.meta.description);
    resetDescriptionScroll();

    requestArtwork(index);

    SF_LOG_I("UI", "Focus '%s' art=%s video=%s",
             g.displayName.c_str(),
             g.meta.boxArtPath.empty() ? "(none)" : g.meta.boxArtPath.c_str(),
             g.meta.videoPath.empty() ? "(none)" : g.meta.videoPath.c_str());
    AppState::instance().video().onSelectionChanged(g.meta.videoPath);
}

void GameListView::runPreviewTransition(size_t index, int direction)
{
    applyPreviewContent(index);

    const auto mode = Config::instance().carouselTransition();
    previewCard->alpha.stop();
    previewCard->setTranslationX(0.f);
    setImageZoom(boxArt, 1.f);
    if (mode == CarouselTransition::None || direction == 0) {
        previewCard->setAlpha(1.f);
        return;
    }

    const bool slide = mode == CarouselTransition::Slide;
    const float slideDistance = slide ? direction * 48.f : 0.f;
    if (slideDistance != 0.f)
        previewCard->setTranslationX(slideDistance);

    const float startAlpha =
        (mode == CarouselTransition::Fade || mode == CarouselTransition::Crossfade ||
         mode == CarouselTransition::Zoom)
            ? 0.f
            : 0.4f;
    previewCard->alpha.reset(startAlpha);
    previewCard->alpha.addStep(1.f, 220, brls::EasingFunction::quadraticOut);

    const bool zoom = mode == CarouselTransition::Zoom;
    if (zoom)
        setImageZoom(boxArt, 1.1f);

    auto alive = alive_;
    previewCard->alpha.setTickCallback([this, alive, slideDistance, zoom]() {
        if (!*alive)
            return;
        const float t = previewCard->alpha.getValue();
        if (slideDistance != 0.f)
            previewCard->setTranslationX(slideDistance * (1.f - t));
        if (zoom)
            setImageZoom(boxArt, 1.1f - 0.1f * t);
        invalidate();
    });
    previewCard->alpha.setEndCallback([this, alive](bool finished) {
        if (!*alive)
            return;
        previewCard->setTranslationX(0.f);
        setImageZoom(boxArt, 1.f);
        if (finished)
            previewCard->setAlpha(1.f);
        invalidate();
    });
    previewCard->alpha.start();
}

void GameListView::onGameFocused(size_t index, int direction)
{
    if (index >= games_.size())
        return;

    focusedIndex_ = index;
    initialFocus_ = index;
    runPreviewTransition(index, direction);
}

void GameListView::requestArtwork(size_t index)
{
    if (index >= games_.size())
        return;

    const auto& g = games_[index];
    std::string path = resolveGameListArtPath(g.meta);
    if (path.empty() || !FileSystem::exists(path)) {
        applyGameArtPlaceholder(boxArt);
        return;
    }

    auto alive = alive_;
    AppState::instance().textures().request(path, [this, index, path, alive](TextureHandle handle) {
        if (!*alive || index != focusedIndex_)
            return;
        if (handle.id == 0 || !FileSystem::exists(path)) {
            applyGameArtPlaceholder(boxArt);
            return;
        }
        boxArt->setImageFromFile(path);
    });
}

} // namespace sf::ui
