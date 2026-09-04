#pragma once

#include "cloud/CloudSaveService.hpp"

#include <atomic>
#include <borealis/core/i18n.hpp>
#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>

namespace sf::ui {

using namespace brls::literals;

class CloudRestoreProgressView : public brls::Box {
public:
    CloudRestoreProgressView(std::string backupId, std::string backupName);
    ~CloudRestoreProgressView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    bool isFinished() const { return finished_; }

    /** @return true if back was consumed (still running). */
    bool handleBack();

    static void present(std::string backupId, std::string backupName);

private:
    void startRestore();
    void onRestoreDone(sf::cloud::CloudRestoreResult result);
    void onProgress(const sf::cloud::CloudRestoreProgress& progress);
    void appendLog(const std::string& line, int tone = 0);
    void scrollLogToBottom(bool animated);
    void tickLogScroll(float delta);
    void tickAbortHold(float delta);
    void setProgressFraction(float fraction);
    void updateTiming();
    void updateAbortHint();

    std::string backupId_;
    std::string backupName_;
    bool running_ = false;
    bool finished_ = false;
    bool autoScrollLog_ = true;
    bool abortTriggered_ = false;
    float backHoldSeconds_ = 0.f;
    brls::Time manualScrollIdleUntil_ = 0;
    brls::Time lastFrameTime_ = 0;
    brls::Time batchStartTime_ = 0;
    size_t filesDone_ = 0;
    static constexpr size_t kMaxLogLines = 160;
    static constexpr float kAbortHoldSeconds = 3.f;

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::atomic<bool> abortRequested_{false};

    BRLS_BIND(brls::Label, titleLabel, "cloud_restore/title");
    BRLS_BIND(brls::Label, counterLabel, "cloud_restore/counter");
    BRLS_BIND(brls::Box, progressTrack, "cloud_restore/progress_track");
    BRLS_BIND(brls::Box, progressFill, "cloud_restore/progress_fill");
    BRLS_BIND(brls::Label, timingLabel, "cloud_restore/timing");
    BRLS_BIND(brls::Label, currentLabel, "cloud_restore/current");
    BRLS_BIND(brls::Label, phaseLabel, "cloud_restore/phase");
    BRLS_BIND(brls::ScrollingFrame, logScroller, "cloud_restore/log_scroll");
    BRLS_BIND(brls::Box, logList, "cloud_restore/log_list");
    BRLS_BIND(brls::Label, hintLabel, "cloud_restore/hint");
    BRLS_BIND(brls::Button, closeBtn, "cloud_restore/close");
};

class CloudRestoreProgressActivity : public brls::Activity {
public:
    explicit CloudRestoreProgressActivity(CloudRestoreProgressView* view)
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
        for (auto btn : block)
            view_->registerAction("Block", btn, consume);

        view_->registerAction(
            "hints/back"_i18n, brls::BUTTON_B,
            [this](brls::View*) {
                if (!view_->isFinished() && view_->handleBack())
                    return true;
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return true;
            },
            false, false, brls::SOUND_BACK);

        brls::Application::giveFocus(view_);
    }

private:
    CloudRestoreProgressView* view_ = nullptr;
};

} // namespace sf::ui
