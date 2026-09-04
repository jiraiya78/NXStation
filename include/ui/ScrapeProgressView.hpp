#pragma once

#include "app/Models.hpp"
#include "scraper/ScrapeTypes.hpp"

#include <borealis/core/i18n.hpp>
#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sf::ui {

using namespace brls::literals;

class ScrapeProgressView : public brls::Box {
public:
    ScrapeProgressView(std::string systemId, ScrapeMode mode, std::vector<sf::GameItem> games);
    ~ScrapeProgressView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    /** @return true if back was consumed (abort in progress). */
    bool handleBack();

    void setOnDismiss(std::function<void()> callback) { onDismiss_ = std::move(callback); }

private:
    void startBatch();
    void updateProgress(const sf::ScrapeProgress& progress);
    void onBatchDone(size_t succeeded, size_t failed, size_t skipped, bool aborted);
    void appendLog(const std::string& line, int tone = 0);
    void scrollLogToBottom(bool animated);
    void tickLogScroll(float delta);
    void resetAssetStatus();
    void setAssetField(const std::string& asset, const std::string& status, int tone);
    void setProgressFraction(float fraction);
    void updateTiming();

    std::string systemId_;
    ScrapeMode mode_;
    std::vector<sf::GameItem> games_;
    bool running_ = false;
    bool finished_ = false;
    bool autoScrollLog_ = true;
    brls::Time manualScrollIdleUntil_ = 0;
    brls::Time lastFrameTime_ = 0;
    std::string assetBoxStatus_ = "—";
    std::string assetSnapStatus_ = "—";
    std::string assetVideoStatus_ = "—";
    brls::Time batchStartTime_ = 0;
    size_t completedCount_ = 0;
    size_t totalCount_ = 0;
    size_t lastProgressIndex_ = 0;
    static constexpr size_t kMaxLogLines = 120;

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::function<void()> onDismiss_;
    bool scrapeKeepAwake_ = false;

    BRLS_BIND(brls::Label, titleLabel, "scrape/title");
    BRLS_BIND(brls::Label, counterLabel, "scrape/counter");
    BRLS_BIND(brls::Box, progressTrack, "scrape/progress_track");
    BRLS_BIND(brls::Box, progressFill, "scrape/progress_fill");
    BRLS_BIND(brls::Label, timingLabel, "scrape/timing");
    BRLS_BIND(brls::Label, gameLabel, "scrape/game");
    BRLS_BIND(brls::Label, assetStatusLabel, "scrape/asset_status");
    BRLS_BIND(brls::Label, phaseLabel, "scrape/phase");
    BRLS_BIND(brls::ScrollingFrame, logScroller, "scrape/log_scroll");
    BRLS_BIND(brls::Box, logList, "scrape/log_list");
    BRLS_BIND(brls::Label, hintLabel, "scrape/hint");
    BRLS_BIND(brls::Button, abortBtn, "scrape/abort");
};

} // namespace sf::ui

namespace sf::ui {

/** Activity for scrape progress — B aborts while running, pops when finished. */
class ScrapeProgressActivity : public brls::Activity {
public:
    explicit ScrapeProgressActivity(ScrapeProgressView* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onContentAvailable() override
    {
        if (!view_)
            return;

        auto consume = [](brls::View*) { return true; };
        const brls::ControllerButton block[] = {
            brls::ControllerButton::BUTTON_A,
            brls::ControllerButton::BUTTON_X,
            brls::ControllerButton::BUTTON_Y,
            brls::ControllerButton::BUTTON_UP,
            brls::ControllerButton::BUTTON_DOWN,
            brls::ControllerButton::BUTTON_LEFT,
            brls::ControllerButton::BUTTON_RIGHT,
            brls::ControllerButton::BUTTON_LB,
            brls::ControllerButton::BUTTON_RB,
            brls::ControllerButton::BUTTON_START,
        };
        for (auto btn : block) {
            view_->registerAction("Block", btn, consume);
        }

        view_->registerAction(
            "hints/back"_i18n, brls::BUTTON_B,
            [this](brls::View*) {
                if (view_->handleBack())
                    return true;
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return true;
            },
            false, false, brls::SOUND_BACK);

        brls::Application::giveFocus(view_);
    }

private:
    ScrapeProgressView* view_ = nullptr;
};

} // namespace sf::ui
