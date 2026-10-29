#include "ui/RomMDownloadProgressView.hpp"
#include "ui/ThemeManager.hpp"

namespace sf::ui {

RomMDownloadProgressView::RomMDownloadProgressView(std::string gameName,
                                                   std::shared_ptr<std::atomic<bool>> abort)
    : gameName_(std::move(gameName))
    , abort_(std::move(abort))
{
    auto& theme = ThemeManager::instance();

    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setBackgroundColor(theme.color("brls/background"));
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setFocusable(true);
    this->setHideHighlight(true);

    titleLabel_ = new brls::Label();
    titleLabel_->setFontSize(32);
    titleLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    titleLabel_->setTextColor(theme.color("brls/text"));
    titleLabel_->setText("Downloading from RomM");
    this->addView(titleLabel_);

    auto* gameLabel = new brls::Label();
    gameLabel->setFontSize(24);
    gameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    gameLabel->setTextColor(theme.color("nxstation/detail_text"));
    gameLabel->setMarginTop(12);
    gameLabel->setText(gameName_);
    this->addView(gameLabel);

    progressTrack_ = new brls::Box();
    progressTrack_->setWidth(kTrackWidth);
    progressTrack_->setHeight(8);
    progressTrack_->setMarginTop(24);
    progressTrack_->setBackgroundColor(theme.color("nxstation/card_bg"));
    this->addView(progressTrack_);

    progressFill_ = new brls::Box();
    progressFill_->setHeight(8);
    progressFill_->setWidth(0);
    progressFill_->setBackgroundColor(theme.color("brls/accent"));
    this->addView(progressFill_);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(20);
    statusLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    statusLabel_->setTextColor(theme.color("nxstation/detail_text"));
    statusLabel_->setMarginTop(16);
    statusLabel_->setText("Starting download…");
    this->addView(statusLabel_);
}

void RomMDownloadProgressView::willAppear(bool resetState)
{
    *alive_ = true;
    brls::Box::willAppear(resetState);
}

void RomMDownloadProgressView::willDisappear(bool resetState)
{
    *alive_ = false;
    if (onDismiss_)
        onDismiss_();
    brls::Box::willDisappear(resetState);
}

void RomMDownloadProgressView::setStatus(const std::string& status)
{
    if (!*alive_)
        return;
    statusLabel_->setText(status);
}

void RomMDownloadProgressView::setPercent(int percent)
{
    if (!*alive_)
        return;
    percent_ = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    progressFill_->setWidth(kTrackWidth * static_cast<float>(percent_) / 100.0f);
}

void RomMDownloadProgressView::finish(bool ok, const std::string& message)
{
    if (!*alive_)
        return;

    finished_ = true;
    percent_ = ok ? 100 : percent_;
    progressFill_->setWidth(kTrackWidth * static_cast<float>(percent_) / 100.0f);
    statusLabel_->setText(message.empty() ? (ok ? "Download complete" : "Download failed") : message);
    statusLabel_->setTextColor(ThemeManager::instance().color(ok ? "success" : "error"));
}

bool RomMDownloadProgressView::handleBack()
{
    if (finished_)
        return false;

    // First press requests an abort; the worker completes the current file and calls back.
    if (abort_)
        abort_->store(true);
    setStatus("Aborting…");
    return true;
}

} // namespace sf::ui
