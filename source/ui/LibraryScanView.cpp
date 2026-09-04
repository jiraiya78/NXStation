#include "ui/LibraryScanView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/ThemeManager.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "util/Logger.hpp"

namespace sf::ui {

LibraryScanView::LibraryScanView(LibraryScanScope scope, std::string systemId, bool forceRescan)
    : scope_(scope)
    , systemId_(std::move(systemId))
    , forceRescan_(forceRescan)
{
    auto& theme = ThemeManager::instance();
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setBackgroundColor(theme.color("brls/background"));
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setFocusable(true);
    this->setHideHighlight(true);

    systemLabel_ = new brls::Label();
    systemLabel_->setFontSize(32);
    systemLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    systemLabel_->setTextColor(theme.color("brls/text"));
    systemLabel_->setText("Scanning library…");
    this->addView(systemLabel_);

    progressLabel_ = new brls::Label();
    progressLabel_->setFontSize(22);
    progressLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    progressLabel_->setTextColor(theme.color("nxstation/detail_text"));
    progressLabel_->setMarginTop(16);
    progressLabel_->setText("Preparing…");
    this->addView(progressLabel_);
}

LibraryScanView::~LibraryScanView()
{
    *alive_ = false;
}

void LibraryScanView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    brls::Application::giveFocus(this);
    if (!running_)
        startScan();
}

void LibraryScanView::willDisappear(bool resetState)
{
    *alive_ = false;
    brls::Box::willDisappear(resetState);
}

void LibraryScanView::startScan()
{
    if (running_)
        return;
    running_ = true;

    // Pick up user edits to roms_config.json without requiring a full app restart.
    Config::instance().reload();

    progressCurrent_.store(0, std::memory_order_relaxed);
    progressTotal_.store(0, std::memory_order_relaxed);
    lastShownCurrent_ = static_cast<size_t>(-1);
    lastShownTotal_ = static_cast<size_t>(-1);

    auto alive = alive_;
    AppState::instance().pool().enqueue([this, alive]() {
        auto progress = [this, alive](const std::string& systemName, const std::string& systemId,
                                      size_t current, size_t total) {
            if (!*alive)
                return;
            {
                const std::string name = systemName.empty() ? systemId : systemName;
                std::lock_guard<std::mutex> lock(nameMutex_);
                if (progressSystemName_ != name) {
                    progressSystemName_ = name;
                    nameChanged_.store(true, std::memory_order_relaxed);
                }
            }
            progressCurrent_.store(current, std::memory_order_relaxed);
            progressTotal_.store(total, std::memory_order_relaxed);
        };

        if (scope_ == LibraryScanScope::AllSystems)
            AppState::instance().scanAllSystems(forceRescan_, progress);
        else
            AppState::instance().scanSystem(systemId_, forceRescan_, progress);

        brls::sync([this, alive]() {
            if (!*alive)
                return;
            onScanFinished();
        });
    });
}

void LibraryScanView::frame(brls::FrameContext* ctx)
{
    if (running_) {
        if (nameChanged_.exchange(false, std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(nameMutex_);
            systemLabel_->setText(progressSystemName_);
        }

        const size_t current = progressCurrent_.load(std::memory_order_relaxed);
        const size_t total = progressTotal_.load(std::memory_order_relaxed);
        if (current != lastShownCurrent_ || total != lastShownTotal_) {
            lastShownCurrent_ = current;
            lastShownTotal_ = total;
            progressLabel_->setText("Processing " + std::to_string(current) + "/" +
                                    std::to_string(total) + " games");
        }
    }

    brls::Box::frame(ctx);
}

void LibraryScanView::onScanFinished()
{
    running_ = false;
    systemLabel_->setText("Scan complete");
    progressLabel_->setText("");

    if (scope_ == LibraryScanScope::AllSystems && markLibraryCompleted_) {
        Config::instance().setLibraryScanCompleted(true);
        Config::instance().saveUserSettings();
    }

    SystemsTab::requestRefreshAfterSettings();

    if (onComplete_) {
        auto cb = std::move(onComplete_);
        onComplete_ = nullptr;
        cb();
    }
}

void LibraryScanView::presentAll(bool forceRescan)
{
    auto* view = new LibraryScanView(LibraryScanScope::AllSystems, {}, forceRescan);
    view->setOnComplete([] {
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        brls::Application::notify("Library scan complete");
    });
    PushedActivity::push(new LibraryScanActivity(view));
}

void LibraryScanView::presentSystem(const std::string& systemId, bool forceRescan,
                                    std::function<void()> onFinished)
{
    auto* view = new LibraryScanView(LibraryScanScope::SingleSystem, systemId, forceRescan);
    view->setOnComplete([onFinished = std::move(onFinished)] {
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        brls::Application::notify("System scan complete");
        if (onFinished)
            onFinished();
    });
    PushedActivity::push(new LibraryScanActivity(view));
}

} // namespace sf::ui
