#include "ui/CloudRestoreProgressView.hpp"
#include "cloud/CloudSaveService.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "util/ActionLog.hpp"
#include "util/CloudLog.hpp"
#include "util/Logger.hpp"
#include "util/Network.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <nanovg.h>

namespace sf::ui {

namespace {

#ifdef __SWITCH__
void beginRestoreKeepAwake()
{
    appletSetAutoSleepDisabled(true);
    appletSetMediaPlaybackState(true);
}

void endRestoreKeepAwake()
{
    appletSetMediaPlaybackState(false);
    appletSetAutoSleepDisabled(false);
}
#endif

enum class LogTone {
    Neutral = 0,
    Progress,
    Success,
    Failure,
    Warning,
};

NVGcolor logColor(LogTone tone)
{
    auto& theme = ThemeManager::instance();
    switch (tone) {
    case LogTone::Success:
        return theme.color("nxstation/log_success");
    case LogTone::Failure:
        return theme.color("nxstation/log_failure");
    case LogTone::Progress:
        return theme.color("nxstation/log_progress");
    case LogTone::Warning:
        return theme.color("nxstation/log_failure");
    case LogTone::Neutral:
    default:
        return theme.color("nxstation/log_neutral");
    }
}

std::string formatDuration(double seconds)
{
    if (seconds < 0.0 || seconds > 359999.0)
        return "--:--";

    const int total = static_cast<int>(seconds + 0.5);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;

    char buf[16];
    if (hours > 0)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", hours, minutes, secs);
    else
        std::snprintf(buf, sizeof(buf), "%02d:%02d", minutes, secs);
    return buf;
}

} // namespace

CloudRestoreProgressView::CloudRestoreProgressView(std::string backupId, std::string backupName)
    : backupId_(std::move(backupId))
    , backupName_(std::move(backupName))
{
    this->inflateFromXMLRes("xml/views/cloud_restore_progress.xml");
    this->setBackgroundColor(ThemeManager::instance().color("nxstation/overlay_bg"));
    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    logScroller->setScrollingIndicatorVisible(true);

    titleLabel->setText("Cloud Restore");
    titleLabel->setTextColor(ThemeManager::instance().color("brls/text"));
    currentLabel->setTextColor(ThemeManager::instance().color("brls/text"));
    counterLabel->setText("Preparing…");
    currentLabel->setText(backupName_);
    timingLabel->setText("Elapsed --:--");
    setProgressFraction(0.f);
    phaseLabel->setText("Checking save locations…");
    hintLabel->setText(
        "NXStation backup only — not interchangeable with RetroArch cloud backups.\n"
        "Hold B for 3 seconds to abort · Right stick scrolls log");

    closeBtn->setFocusable(false);
    closeBtn->setState(brls::ButtonState::DISABLED);
    closeBtn->registerClickAction([this](brls::View*) {
        if (!finished_)
            return true;
        SF_LOG_ACTION("CloudRestore/Close");
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        return true;
    });
}

CloudRestoreProgressView::~CloudRestoreProgressView()
{
    *alive_ = false;
#ifdef __SWITCH__
    if (running_ && !finished_)
        endRestoreKeepAwake();
#endif
    abortRequested_.store(true);
}

bool CloudRestoreProgressView::handleBack()
{
    if (running_ && !finished_)
        return true;
    return false;
}

void CloudRestoreProgressView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    lastFrameTime_ = 0;
    autoScrollLog_ = true;
    brls::Application::giveFocus(this);
    if (!running_ && !finished_) {
        brls::sync([this]() {
            if (!*alive_ || running_ || finished_)
                return;
            startRestore();
        });
    }
}

void CloudRestoreProgressView::willDisappear(bool resetState)
{
    *alive_ = false;
#ifdef __SWITCH__
    if (running_ && !finished_)
        endRestoreKeepAwake();
#endif
    abortRequested_.store(true);
    brls::Box::willDisappear(resetState);
}

void CloudRestoreProgressView::setProgressFraction(float fraction)
{
    const float pct = std::clamp(fraction, 0.f, 1.f) * 100.f;
    progressFill->setWidthPercentage(pct);
}

void CloudRestoreProgressView::updateTiming()
{
    if (batchStartTime_ == 0)
        return;

    const double elapsed =
        static_cast<double>(brls::getCPUTimeUsec() - batchStartTime_) / 1000000.0;
    std::string text = "Elapsed " + formatDuration(elapsed);
    if (finished_)
        text += "   ·   Done";
    timingLabel->setText(text);
}

void CloudRestoreProgressView::updateAbortHint()
{
    if (finished_) {
        hintLabel->setTextColor(ThemeManager::instance().color("nxstation/muted_text"));
        hintLabel->setText("Press B or Close to return · Right stick scrolls log");
        return;
    }

    hintLabel->setTextColor(ThemeManager::instance().color("nxstation/log_failure"));
    if (backHoldSeconds_ <= 0.f) {
        hintLabel->setText(
            "NXStation backup only — not interchangeable with RetroArch cloud backups.\n"
            "Hold B for 3 seconds to abort (partial restore may leave mixed saves) · "
            "Right stick scrolls log");
        return;
    }

    const int pct = static_cast<int>((backHoldSeconds_ / kAbortHoldSeconds) * 100.f);
    hintLabel->setText("Aborting in " + std::to_string(pct) + "% — release B to cancel");
}

void CloudRestoreProgressView::appendLog(const std::string& line, int tone)
{
    auto* row = new brls::Label();
    row->setText(line);
    row->setFontSize(18);
    row->setSingleLine(false);
    row->setLineHeight(1.25f);
    row->setTextColor(logColor(static_cast<LogTone>(tone)));
    logList->addView(row);

    while (logList->getChildren().size() > kMaxLogLines) {
        auto& children = logList->getChildren();
        logList->removeView(children.front(), true);
    }

    if (autoScrollLog_)
        scrollLogToBottom(false);
}

void CloudRestoreProgressView::scrollLogToBottom(bool animated)
{
    const float viewH = logScroller->getHeight();
    const float contentH = logList->getHeight();
    const float maxOffset = std::max(0.f, contentH - viewH);
    logScroller->setContentOffsetY(maxOffset, animated);
}

void CloudRestoreProgressView::tickLogScroll(float delta)
{
    const auto& pad = brls::Application::getControllerState();
    const float stickY = pad.axes[brls::RIGHT_Y];
    const float deadzone = 0.2f;

    if (std::abs(stickY) > deadzone) {
        autoScrollLog_ = false;
        manualScrollIdleUntil_ = brls::getCPUTimeUsec() + 1000000;

        const float viewH = logScroller->getHeight();
        const float contentH = logList->getHeight();
        const float maxOffset = std::max(0.f, contentH - viewH);
        float offset = logScroller->getContentOffsetY();
        offset += stickY * 360.f * delta;
        logScroller->setContentOffsetY(std::clamp(offset, 0.f, maxOffset), false);
        return;
    }

    if (finished_)
        return;

    if (!autoScrollLog_ && brls::getCPUTimeUsec() >= manualScrollIdleUntil_)
        autoScrollLog_ = true;

    if (autoScrollLog_)
        scrollLogToBottom(false);
}

void CloudRestoreProgressView::tickAbortHold(float delta)
{
    if (!running_ || finished_ || abortTriggered_)
        return;

    const auto& pad = brls::Application::getControllerState();
    if (pad.buttons[brls::BUTTON_B]) {
        backHoldSeconds_ += delta;
        if (backHoldSeconds_ >= kAbortHoldSeconds) {
            abortTriggered_ = true;
            abortRequested_.store(true);
            phaseLabel->setText("Aborting…");
            appendLog("Abort requested — stopping after current file",
                      static_cast<int>(LogTone::Warning));
        }
    } else {
        backHoldSeconds_ = 0.f;
    }

    updateAbortHint();
}

void CloudRestoreProgressView::frame(brls::FrameContext* ctx)
{
    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    if (delta > 0.f) {
        tickLogScroll(delta);
        tickAbortHold(delta);
    }

    if (running_ && !finished_)
        updateTiming();

    brls::Box::frame(ctx);
}

void CloudRestoreProgressView::startRestore()
{
    if (!Network::isAvailable()) {
        appendLog("Waiting for Wi‑Fi / network…", static_cast<int>(LogTone::Progress));
        if (!Network::waitForConnection(30)) {
            counterLabel->setText("No network");
            phaseLabel->setText("Failed");
            appendLog("Network unavailable — connect in system settings, then retry",
                      static_cast<int>(LogTone::Failure));
            finished_ = true;
            closeBtn->setText("Close");
            closeBtn->setFocusable(true);
            closeBtn->setState(brls::ButtonState::ENABLED);
            updateAbortHint();
            return;
        }
    }

    running_ = true;
#ifdef __SWITCH__
    beginRestoreKeepAwake();
#endif
    batchStartTime_ = brls::getCPUTimeUsec();
    setProgressFraction(0.f);
    updateTiming();
    updateAbortHint();

    appendLog("NXStation cloud backup — do not use RetroArch's own backup format interchangeably.",
              static_cast<int>(LogTone::Warning));
    appendLog("Starting restore of " + backupName_, static_cast<int>(LogTone::Neutral));
    CloudLog::write("UI: restore progress opened for " + backupName_);

    auto alive = alive_;
    sf::cloud::CloudSaveService::instance().restoreNow(
        backupId_, backupName_,
        [this, alive](const sf::cloud::CloudRestoreProgress& progress) {
            if (!*alive)
                return;
            onProgress(progress);
        },
        &abortRequested_,
        [this, alive](sf::cloud::CloudRestoreResult result) {
            if (!*alive)
                return;
            onRestoreDone(result);
        });
}

void CloudRestoreProgressView::onProgress(const sf::cloud::CloudRestoreProgress& progress)
{
    if (progress.percent > 0)
        setProgressFraction(static_cast<float>(progress.percent) / 100.f);

    LogTone tone = LogTone::Neutral;
    switch (progress.kind) {
    case sf::cloud::CloudRestoreProgress::Kind::Info:
        tone = LogTone::Progress;
        phaseLabel->setText(progress.message);
        currentLabel->setText(progress.detail);
        counterLabel->setText("Reading RetroArch paths from retroarch.cfg");
        appendLog(progress.message + ": " + progress.detail, static_cast<int>(tone));
        break;
    case sf::cloud::CloudRestoreProgress::Kind::PreBackup:
        tone = LogTone::Progress;
        phaseLabel->setText(progress.message);
        currentLabel->setText(progress.detail.empty() ? "—" : progress.detail);
        counterLabel->setText("Local safety backup");
        appendLog(progress.message + (progress.detail.empty() ? "" : " — " + progress.detail),
                  static_cast<int>(tone));
        break;
    case sf::cloud::CloudRestoreProgress::Kind::Download:
        tone = LogTone::Progress;
        phaseLabel->setText(progress.message);
        currentLabel->setText(backupName_);
        counterLabel->setText("Downloading from Google Drive");
        appendLog(progress.message, static_cast<int>(tone));
        break;
    case sf::cloud::CloudRestoreProgress::Kind::Extract:
        if (progress.skipped)
            tone = LogTone::Neutral;
        else if (progress.fileOk)
            tone = LogTone::Success;
        else
            tone = LogTone::Failure;
        phaseLabel->setText(progress.message);
        currentLabel->setText(progress.detail.empty() ? "—" : progress.detail);
        ++filesDone_;
        counterLabel->setText("Files processed: " + std::to_string(filesDone_));
        appendLog(progress.message + ": " + progress.detail, static_cast<int>(tone));
        break;
    case sf::cloud::CloudRestoreProgress::Kind::Summary:
        tone = progress.fileOk ? LogTone::Success : LogTone::Failure;
        phaseLabel->setText(progress.fileOk ? "Complete" : "Finished");
        currentLabel->setText("—");
        counterLabel->setText(progress.message);
        appendLog(progress.message, static_cast<int>(tone));
        break;
    }
}

void CloudRestoreProgressView::onRestoreDone(sf::cloud::CloudRestoreResult result)
{
    running_ = false;
    finished_ = true;
    autoScrollLog_ = false;
    backHoldSeconds_ = 0.f;
#ifdef __SWITCH__
    endRestoreKeepAwake();
#endif

    setProgressFraction(1.f);
    updateTiming();
    updateAbortHint();
    phaseLabel->setText(result.aborted ? "Aborted"
                                       : (result.ok ? "Complete" : "Finished with errors"));
    counterLabel->setText(result.message);

    if (!result.message.empty() && result.ok)
        brls::Application::notify(result.message);
    else if (!result.ok)
        brls::Application::notify(result.aborted ? "Restore aborted" : ("Restore failed: " + result.message));

    closeBtn->setText("Close");
    closeBtn->setFocusable(true);
    closeBtn->setState(brls::ButtonState::ENABLED);
    brls::Application::giveFocus(this);

    CloudLog::writef("UI: restore finished ok=%d aborted=%d — %s", result.ok ? 1 : 0,
                     result.aborted ? 1 : 0, result.message.c_str());
    SF_LOG_I("UI", "Cloud restore %s", result.message.c_str());
}

void CloudRestoreProgressView::present(std::string backupId, std::string backupName)
{
    auto* view = new CloudRestoreProgressView(std::move(backupId), std::move(backupName));
    PushedActivity::push(new CloudRestoreProgressActivity(view));
}

} // namespace sf::ui
