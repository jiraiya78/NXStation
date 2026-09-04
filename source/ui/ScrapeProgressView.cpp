#include "ui/ScrapeProgressView.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "scraper/ScraperService.hpp"
#include "util/ActionLog.hpp"
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
void beginScrapeKeepAwake()
{
    appletSetAutoSleepDisabled(true);
    appletSetMediaPlaybackState(true);
}

void endScrapeKeepAwake()
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
};

std::string modeLabel(ScrapeMode mode)
{
    switch (mode) {
    case ScrapeMode::MissingArtOnly:
        return "Missing art only";
    case ScrapeMode::Single:
        return "Single game";
    default:
        return "Full scrape";
    }
}

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
    case LogTone::Neutral:
    default:
        return theme.color("nxstation/log_neutral");
    }
}

bool isAssetPhase(const std::string& phase)
{
    return phase == "Box art" || phase == "Thumbnail" || phase == "Video"
           || phase == "Manual";
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

LogTone toneForAsset(const std::string& detail, bool hasResult, bool success)
{
    if (!hasResult || detail == "Downloading…")
        return LogTone::Progress;
    if (success || detail == "OK")
        return LogTone::Success;
    if (detail == "Not available" || detail.find("Skipped") != std::string::npos)
        return LogTone::Neutral;
    return LogTone::Failure;
}

} // namespace

ScrapeProgressView::ScrapeProgressView(std::string systemId, ScrapeMode mode,
                                         std::vector<sf::GameItem> games)
    : systemId_(std::move(systemId))
    , mode_(mode)
    , games_(std::move(games))
{
    this->inflateFromXMLRes("xml/views/scrape_progress.xml");
    this->setBackgroundColor(ThemeManager::instance().color("nxstation/overlay_bg"));
    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    logScroller->setScrollingIndicatorVisible(true);

    const SystemConfig* sys = Config::instance().findSystem(systemId_);
    titleLabel->setText(sys ? sys->name : systemId_);
    titleLabel->setTextColor(ThemeManager::instance().color("brls/text"));
    gameLabel->setTextColor(ThemeManager::instance().color("brls/text"));
    counterLabel->setText("Preparing…");
    gameLabel->setText("—");
    timingLabel->setText("Elapsed --:--   ·   Remaining --:--");
    setProgressFraction(0.f);
    resetAssetStatus();
    phaseLabel->setText(modeLabel(mode_));
    hintLabel->setText("Press B to abort · Right stick scrolls log");

    abortBtn->setFocusable(false);
    abortBtn->setState(brls::ButtonState::DISABLED);

    abortBtn->registerClickAction([this](brls::View*) {
        if (!finished_)
            return true;
        SF_LOG_ACTION("Scrape/Close");
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        return true;
    });
}

bool ScrapeProgressView::handleBack()
{
    if (running_ && !finished_) {
        SF_LOG_ACTION("Scrape/Abort");
        AppState::instance().scraper().requestAbort();
        phaseLabel->setText("Aborting…");
        abortBtn->setState(brls::ButtonState::DISABLED);
        return true;
    }
    return false;
}

ScrapeProgressView::~ScrapeProgressView()
{
    *alive_ = false;
#ifdef __SWITCH__
    if (scrapeKeepAwake_)
        endScrapeKeepAwake();
#endif
    if (running_ && !finished_)
        AppState::instance().scraper().requestAbort();
}

void ScrapeProgressView::willAppear(bool resetState)
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
            startBatch();
        });
    }
}

void ScrapeProgressView::willDisappear(bool resetState)
{
    *alive_ = false;
#ifdef __SWITCH__
    if (scrapeKeepAwake_) {
        endScrapeKeepAwake();
        scrapeKeepAwake_ = false;
    }
#endif
    if (running_ && !finished_)
        AppState::instance().scraper().requestAbort();

    if (onDismiss_) {
        auto cb = std::move(onDismiss_);
        onDismiss_ = nullptr;
        brls::sync(std::move(cb));
    }

    brls::Box::willDisappear(resetState);
}

void ScrapeProgressView::resetAssetStatus()
{
    assetBoxStatus_ = "—";
    assetSnapStatus_ = "—";
    assetVideoStatus_ = "—";
    assetStatusLabel->setText("Box: —   Thumb: —   Video: —");
}

void ScrapeProgressView::setAssetField(const std::string& asset, const std::string& status,
                                       int /*tone*/)
{
    if (asset == "Box art")
        assetBoxStatus_ = status;
    else if (asset == "Thumbnail")
        assetSnapStatus_ = status;
    else if (asset == "Video")
        assetVideoStatus_ = status;

    assetStatusLabel->setText("Box: " + assetBoxStatus_ + "   Thumb: " + assetSnapStatus_
                              + "   Video: " + assetVideoStatus_);
}

void ScrapeProgressView::setProgressFraction(float fraction)
{
    const float pct = std::clamp(fraction, 0.f, 1.f) * 100.f;
    progressFill->setWidthPercentage(pct);
}

void ScrapeProgressView::updateTiming()
{
    if (batchStartTime_ == 0)
        return;

    const double elapsed =
        static_cast<double>(brls::getCPUTimeUsec() - batchStartTime_) / 1000000.0;

    std::string text = "Elapsed " + formatDuration(elapsed);

    if (finished_) {
        text += "   ·   Done";
    } else if (completedCount_ > 0 && totalCount_ > completedCount_) {
        const double perItem = elapsed / static_cast<double>(completedCount_);
        const double remaining =
            perItem * static_cast<double>(totalCount_ - completedCount_);
        text += "   ·   Remaining " + formatDuration(remaining);
        text += "   ·   " + formatDuration(perItem) + " / game";
    } else {
        text += "   ·   Remaining --:--";
    }

    timingLabel->setText(text);
}

void ScrapeProgressView::appendLog(const std::string& line, int tone)
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

void ScrapeProgressView::scrollLogToBottom(bool animated)
{
    const float viewH = logScroller->getHeight();
    const float contentH = logList->getHeight();
    const float maxOffset = std::max(0.f, contentH - viewH);
    logScroller->setContentOffsetY(maxOffset, animated);
}

void ScrapeProgressView::tickLogScroll(float delta)
{
    if (finished_)
        return;

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

    if (!autoScrollLog_ && brls::getCPUTimeUsec() >= manualScrollIdleUntil_)
        autoScrollLog_ = true;

    if (autoScrollLog_)
        scrollLogToBottom(false);
}

void ScrapeProgressView::frame(brls::FrameContext* ctx)
{
    const brls::Time now = brls::getCPUTimeUsec();
    float delta = lastFrameTime_ == 0 ? 0.f : static_cast<float>(now - lastFrameTime_) / 1000000.f;
    lastFrameTime_ = now;
    if (delta < 0.f || delta > 0.5f)
        delta = 0.f;

    if (delta > 0.f)
        tickLogScroll(delta);

    if (running_ && !finished_)
        updateTiming();

    brls::Box::frame(ctx);
}

void ScrapeProgressView::startBatch()
{
    if (games_.empty()) {
        const auto& stored = AppState::instance().gamesFor(systemId_);
        games_.assign(stored.begin(), stored.end());
    }

    if (games_.empty()) {
        counterLabel->setText("No games to scrape");
        phaseLabel->setText("Done");
        hintLabel->setText("Press B to go back");
        finished_ = true;
        abortBtn->setText("Close");
        abortBtn->setFocusable(true);
        abortBtn->setState(brls::ButtonState::ENABLED);
        return;
    }

    if (!Network::isAvailable()) {
        appendLog("Waiting for Wi‑Fi / network…", static_cast<int>(LogTone::Progress));
        if (!Network::waitForConnection(30)) {
            counterLabel->setText("No network");
            phaseLabel->setText("Failed");
            appendLog("Network unavailable — connect in system settings, then retry",
                      static_cast<int>(LogTone::Failure));
            finished_ = true;
            abortBtn->setText("Close");
            abortBtn->setFocusable(true);
            abortBtn->setState(brls::ButtonState::ENABLED);
            return;
        }
    }

    running_ = true;
#ifdef __SWITCH__
    if (!scrapeKeepAwake_) {
        beginScrapeKeepAwake();
        scrapeKeepAwake_ = true;
    }
#endif
    batchStartTime_ = brls::getCPUTimeUsec();
    completedCount_ = 0;
    totalCount_ = games_.size();
    setProgressFraction(0.f);
    updateTiming();
    abortBtn->setText("Abort");
    abortBtn->setState(brls::ButtonState::DISABLED);
    appendLog("Starting " + modeLabel(mode_) + " — " + std::to_string(games_.size()) + " titles",
              static_cast<int>(LogTone::Neutral));

    const SystemConfig* sys = Config::instance().findSystem(systemId_);
    if (!sys) {
        phaseLabel->setText("Error");
        appendLog("Unknown system: " + systemId_, static_cast<int>(LogTone::Failure));
        running_ = false;
        finished_ = true;
        return;
    }

    auto alive = alive_;
    AppState::instance().scraper().runBatchAsync(
        games_, *sys, mode_,
        [this, alive](const sf::ScrapeProgress& progress) {
            if (!*alive)
                return;
            updateProgress(progress);
        },
        [this, alive](size_t succeeded, size_t failed, size_t skipped, bool aborted) {
            if (!*alive)
                return;
            onBatchDone(succeeded, failed, skipped, aborted);
        });
}

void ScrapeProgressView::updateProgress(const sf::ScrapeProgress& progress)
{
    counterLabel->setText(std::to_string(progress.index) + " / " + std::to_string(progress.total)
                          + "   OK " + std::to_string(progress.succeeded) + "   Fail "
                          + std::to_string(progress.failed) + "   Skip "
                          + std::to_string(progress.skipped));
    gameLabel->setText(progress.gameName);

    if (progress.total > 0)
        totalCount_ = progress.total;
    completedCount_ = progress.succeeded + progress.failed + progress.skipped;
    if (totalCount_ > 0)
        setProgressFraction(static_cast<float>(completedCount_)
                            / static_cast<float>(totalCount_));
    updateTiming();

    if (isAssetPhase(progress.phase)) {
        const LogTone tone = toneForAsset(progress.detail, progress.hasResult, progress.success);
        setAssetField(progress.phase, progress.detail, static_cast<int>(tone));
        phaseLabel->setText(progress.phase + ": " + progress.detail);

        if (progress.detail != "Downloading…") {
            std::string line = "[" + std::to_string(progress.index) + "/" + std::to_string(progress.total)
                               + "]   " + progress.phase + ": " + progress.detail;
            appendLog(line, static_cast<int>(tone));
        }
        return;
    }

    if (progress.phase == "Scraping") {
        if (progress.index != lastProgressIndex_) {
            lastProgressIndex_ = progress.index;
            resetAssetStatus();
        }
        phaseLabel->setText(progress.detail);
        return;
    }

    phaseLabel->setText(progress.phase + (progress.detail.empty() ? "" : ": " + progress.detail));

    LogTone tone = LogTone::Neutral;
    if (progress.detail.find("Skipped") != std::string::npos
        || progress.detail == "Not available"
        || progress.phase == "Skipped")
        tone = LogTone::Neutral;
    else if (progress.phase == "OK" || (progress.hasResult && progress.success))
        tone = LogTone::Success;
    else if (progress.phase == "Failed" || (progress.hasResult && !progress.success))
        tone = LogTone::Failure;

    std::string line = "[" + std::to_string(progress.index) + "/" + std::to_string(progress.total)
                       + "] " + progress.gameName;
    if (!progress.phase.empty())
        line += " — " + progress.phase;
    if (!progress.detail.empty())
        line += ": " + progress.detail;
    appendLog(line, static_cast<int>(tone));
}

void ScrapeProgressView::onBatchDone(size_t succeeded, size_t failed, size_t skipped, bool aborted)
{
    running_ = false;
    finished_ = true;
    autoScrollLog_ = false;
#ifdef __SWITCH__
    if (scrapeKeepAwake_) {
        endScrapeKeepAwake();
        scrapeKeepAwake_ = false;
    }
#endif
    completedCount_ = succeeded + failed + skipped;
    setProgressFraction(aborted && totalCount_ > 0
                            ? static_cast<float>(completedCount_)
                                  / static_cast<float>(totalCount_)
                            : 1.f);
    updateTiming();

    std::string summary = aborted ? "Aborted — " : "Finished — ";
    summary += std::to_string(succeeded) + " OK, " + std::to_string(failed) + " failed, "
               + std::to_string(skipped) + " skipped";
    counterLabel->setText(summary);
    phaseLabel->setText(aborted ? "Aborted" : "Complete");
    gameLabel->setText("—");
    resetAssetStatus();
    hintLabel->setText("Press B or Close to return to the game list");
    abortBtn->setText("Close");
    abortBtn->setFocusable(true);
    abortBtn->setState(brls::ButtonState::ENABLED);

    appendLog(summary, aborted ? static_cast<int>(LogTone::Failure)
                                : static_cast<int>(LogTone::Success));
    SF_LOG_I("UI", "Scrape batch %s", summary.c_str());

    if (!aborted) {
        playScrapeCompleteSfx();
        brls::Application::notify(summary);
    }
}

} // namespace sf::ui
