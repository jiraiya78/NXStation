#pragma once

#include "util/IdleTimer.hpp"

#include <borealis.hpp>
#include <vector>

namespace sf::ui {

/** Vertical system list browser (same layout as the game list). */
class SystemsListTab : public brls::Box {
public:
    SystemsListTab();
    ~SystemsListTab() override = default;

    void willAppear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    void onRowFocused(int row);
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, size_t index);
    void openSystemAt(size_t index);
    size_t rowCount() const { return systemIds_.size(); }

private:
    void rebuildSystemIds();
    void registerActions();
    void applyThemeColors();
    void showSystem(size_t index, int direction = 0);
    void applyPreview(size_t index);
    void applyLightPreview(size_t index);
    void resetArtTransform();
    void cancelPreviewTransition();
    void stepBy(int delta, bool repeating);
    void beginPreviewTransition(size_t nextIndex, int direction);
    void beginPreviewFade(size_t nextIndex);
    void beginPreviewSlide(size_t nextIndex, int direction);
    void beginPreviewCrossfade(size_t nextIndex);
    void beginPreviewZoom(size_t nextIndex);
    void tickIdleScreensaver();
    void tickButtonNavigation(float delta);
    void reloadList();

    size_t index_ = 0;
    std::vector<std::string> systemIds_;
    uint32_t themeGenApplied_ = 0;
    IdleTimer idleTimer_;
    brls::Time lastFrameTime_ = 0;
    int navDir_ = 0;
    int dpadHoldDir_ = 0;
    float dpadHoldTime_ = 0.f;
    float dpadRepeatTimer_ = 0.f;
    int dpadRepeatCount_ = 0;
    bool jumping_ = false;
    bool navPreviewDeferred_ = false;
    bool transitioning_ = false;
    brls::Animatable fadeAnim_{0.f};
    brls::Animatable slideAnim_{0.f};

    BRLS_BIND(brls::Label, titleLabel, "list/title");
    BRLS_BIND(brls::Label, countLabel, "list/count");
    BRLS_BIND(brls::Label, hintsLabel, "list/hints");
    BRLS_BIND(brls::Box, previewCard, "list/preview");
    BRLS_BIND(brls::Box, artFrame, "list/art_frame");
    BRLS_BIND(brls::Image, artImage, "list/art");
    BRLS_BIND(brls::Image, artImageNext, "list/art_next");
    BRLS_BIND(brls::ScrollingFrame, detailScroller, "list/detail_scroll");
    BRLS_BIND(brls::Label, systemNameLabel, "list/system_name");
    BRLS_BIND(brls::Label, detailLabel, "list/detail");
    BRLS_BIND(brls::RecyclerFrame, scroller, "list/scroller");
};

} // namespace sf::ui
