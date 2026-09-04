#include "ui/SystemsBrowser.hpp"

#include "ui/SystemBrowserStyle.hpp"
#include "app/Config.hpp"
#include "ui/SystemsBrowserData.hpp"
#include "ui/SystemsListTab.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/ThemeManager.hpp"

#include <atomic>

namespace sf::ui {

namespace {

std::atomic<bool> gBrowserLayoutRefreshRequested{false};

int styleToken()
{
    if (ThemeManager::instance().themeForcesListBrowser())
        return 2;
    return Config::instance().systemBrowserStyle() == SystemBrowserStyle::List ? 1 : 0;
}

} // namespace

SystemsBrowser::SystemsBrowser()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.f);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);
    rebuildChild();
}

brls::View* SystemsBrowser::create()
{
    return new SystemsBrowser();
}

void SystemsBrowser::requestRefreshAfterSettings()
{
    gBrowserLayoutRefreshRequested.store(true);
    browser::requestSystemsDataRefresh();
}

void SystemsBrowser::rebuildChild()
{
    const int token = styleToken();
    if (child_ && token == activeStyle_)
        return;

    if (child_) {
        this->removeView(child_);
        child_ = nullptr;
    }

    activeStyle_ = token;
    if (token == 1 || token == 2)
        child_ = new SystemsListTab();
    else
        child_ = new SystemsTab();

    child_->setGrow(1.f);
    this->addView(child_);
}

void SystemsBrowser::syncChildFocus()
{
    if (child_)
        brls::Application::giveFocus(child_);
}

void SystemsBrowser::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    rebuildChild();
    syncChildFocus();
}

void SystemsBrowser::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        rebuildChild();
        syncChildFocus();
    }

    if (gBrowserLayoutRefreshRequested.exchange(false))
        rebuildChild();

    brls::Box::frame(ctx);
}

} // namespace sf::ui
