#pragma once

#include "app/Models.hpp"
#include "scraper/ScrapeTypes.hpp"
#include "util/IdleTimer.hpp"

#include <borealis.hpp>
#include <borealis/views/recycler.hpp>
#include <memory>
#include <string>
#include <vector>

namespace sf::ui {

class VideoPreviewView;

class GameListView : public brls::Box {
public:
    explicit GameListView(std::string systemId, size_t initialFocus = 0);

    /** Fixed result set (search); rows keep their own systemId for launch and art. */
    GameListView(std::string title, std::vector<sf::GameItem> games, std::string subtitle);

    ~GameListView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    // Internal — used by the row data source, not part of the public API.
    size_t rowCount() const { return games_.size(); }
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, size_t index);
    void onRowFocused(size_t index);
    void onRowClicked(size_t index);
    void trackFocusedRow(brls::View* view) { focusedRowView_ = view; }
    bool deferGameRowHighlight() const { return deferRowHighlight_; }
    void finishDeferredRowHighlight();

private:
    enum class DescriptionPhase {
        HoldTop,
        Scrolling,
        HoldBottom,
    };

    void rebuildList();
    void refreshFromStore();
    void syncGamesFromStore();
    void requestArtwork(size_t index);
    void openOptionsMenu();
    void openScrapeMenu();
    void toggleFavorite(size_t index);
    void launchGame(size_t index);
    void onGameFocused(size_t index, int direction);
    void applyPreviewContent(size_t index);
    void applyLightPreview(size_t index);
    void runPreviewTransition(size_t index, int direction);
    void installVideoPreview();
    void updatePreviewSurface(brls::FrameContext* ctx);
    void resetDescriptionScroll();
    void tickDescriptionScroll(float delta);
    void tickDescriptionStickScroll(float delta);
    void tickAcceleratedNavigation(float delta);
    void stepListNav(int direction, bool repeating);
    void stepPageNav(int direction, bool repeating);
    void setOverlayBlocking(bool blocked);
    void pushOverlayBlock();
    void popOverlayBlock();
    /** Pop overlay after N frames (keeps list blocked while a modal is still closing). */
    void schedulePopOverlayBlock(int frames = 4);
    /** Force-unblock the list (depth reset) after modal UI that may have missed a pop. */
    void clearOverlayBlock();
    void applyThemeColors();
    void pushScrapeProgress(ScrapeMode mode, std::vector<sf::GameItem> games,
                            const std::string& scrapeSystemId = {});
    void jumpToBucket(char bucket);
    void jumpToRandomGame();
    void jumpToIndex(size_t index, bool animated = false);
    void tickIdleScreensaver();
    void registerListActions();
    void renameFocusedGame();
    void confirmDeleteFocusedGame();
    void deleteGame(const sf::GameItem& game);

    std::string systemId_;
    std::vector<sf::GameItem> games_;
    size_t initialFocus_ = 0;
    size_t focusedIndex_ = static_cast<size_t>(-1);
    bool customList_ = false;
    std::string customTitle_;
    std::string customSubtitle_;

    BRLS_BIND(brls::RecyclerFrame, scroller, "games/scroller");
    BRLS_BIND(brls::Label, titleLabel, "games/title");
    BRLS_BIND(brls::Label, countLabel, "games/count");
    BRLS_BIND(brls::Box, artFrame, "games/art_frame");
    BRLS_BIND(brls::Image, boxArt, "games/boxart");
    BRLS_BIND(brls::ScrollingFrame, detailScroller, "games/detail_scroll");
    BRLS_BIND(brls::Label, detailLabel, "games/detail");
    BRLS_BIND(brls::Label, hintsLabel, "games/hints");
    BRLS_BIND(brls::Box, previewCard, "games/preview");

    VideoPreviewView* videoPreview_ = nullptr;
    bool videoVisible_ = false;

    brls::Time lastFrameTime_ = 0;
    DescriptionPhase descriptionPhase_ = DescriptionPhase::HoldTop;
    float descriptionOffset_ = 0.f;
    float descriptionHold_ = 0.f;
    bool manualDescriptionScroll_ = false;

    bool navPreviewDeferred_ = false;
    int navRepeatCount_ = 0;
    int navHoldDir_ = 0;
    float navHoldTime_ = 0.f;
    float navRepeatTimer_ = 0.f;
    int pageHoldDir_ = 0;
    float pageHoldTime_ = 0.f;
    float pageRepeatTimer_ = 0.f;
    int pageRepeatCount_ = 0;

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    bool overlayBlocking_ = false;
    int overlayDepth_ = 0;
    bool deferRowHighlight_ = false;
    bool jumping_ = false;
    brls::View* focusedRowView_ = nullptr;
    uint32_t themeGenApplied_ = 0;
    IdleTimer idleTimer_;
};

} // namespace sf::ui
