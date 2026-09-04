#include "ui/CloudSyncView.hpp"
#include "cloud/CloudSaveService.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "util/ActionLog.hpp"
#include "util/CloudLog.hpp"

#include <mutex>

namespace sf::ui {

CloudSyncView::CloudSyncView()
{
    auto& theme = ThemeManager::instance();
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setBackgroundColor(theme.color("brls/background"));
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setFocusable(true);
    this->setHideHighlight(true);

    stageLabel_ = new brls::Label();
    stageLabel_->setFontSize(32);
    stageLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    stageLabel_->setTextColor(theme.color("brls/text"));
    stageLabel_->setText("Backup Saves to Cloud");
    this->addView(stageLabel_);

    detailLabel_ = new brls::Label();
    detailLabel_->setFontSize(22);
    detailLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    detailLabel_->setTextColor(theme.color("nxstation/detail_text"));
    detailLabel_->setMarginTop(16);
    detailLabel_->setText("Preparing…");
    this->addView(detailLabel_);
}

CloudSyncView::~CloudSyncView()
{
    *alive_ = false;
}

void CloudSyncView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    brls::Application::giveFocus(this);
    if (!running_)
        startSync();
}

void CloudSyncView::willDisappear(bool resetState)
{
    *alive_ = false;
    brls::Box::willDisappear(resetState);
}

void CloudSyncView::frame(brls::FrameContext* ctx)
{
    if (stageChanged_.exchange(false)) {
        std::lock_guard<std::mutex> lock(stageMutex_);
        if (stageLabel_)
            stageLabel_->setText(stageText_);
    }

    const int percent = progressPercent_.load(std::memory_order_relaxed);
    if (percent != lastShownPercent_ && detailLabel_) {
        lastShownPercent_ = percent;
        detailLabel_->setText(percent > 0 ? std::to_string(percent) + "%" : "Preparing…");
    }

    brls::Box::frame(ctx);
}

void CloudSyncView::startSync()
{
    if (running_)
        return;
    running_ = true;

    auto alive = alive_;
    sf::cloud::CloudSaveService::instance().syncNow(
        [this, alive](const std::string& stage, int percent) {
            if (!*alive)
                return;
            {
                std::lock_guard<std::mutex> lock(stageMutex_);
                stageText_ = stage;
            }
            stageChanged_.store(true, std::memory_order_relaxed);
            progressPercent_.store(percent, std::memory_order_relaxed);
        },
        [this, alive](sf::cloud::CloudSyncResult result) {
            if (!*alive)
                return;
            running_ = false;
            if (result.ok)
                brls::Application::notify(result.message);
            else
                brls::Application::notify("Cloud backup failed: " + result.message);

            if (onFinished_)
                onFinished_(result.ok);

            brls::Application::popActivity(brls::TransitionAnimation::FADE);
        });
}

void CloudSyncView::present(std::function<void(bool success)> onFinished)
{
    auto* view = new CloudSyncView();
    view->onFinished_ = std::move(onFinished);
    brls::Application::pushActivity(new CloudSyncActivity(view));
}

} // namespace sf::ui
