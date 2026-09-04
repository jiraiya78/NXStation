#include "ui/SettingsSectionView.hpp"

#include "ui/PushedActivity.hpp"
#include "ui/SettingsHelpers.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"

#include <algorithm>

namespace sf::ui {

SettingsSectionView::SettingsSectionView(std::string title, std::string subtitle,
                                           BuildRowsFn buildRows, std::function<void()> onAppear)
    : title_(std::move(title))
    , subtitle_(std::move(subtitle))
    , buildRows_(std::move(buildRows))
    , onAppear_(std::move(onAppear))
{
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    this->setFocusable(false);
    this->setBackgroundColor(ThemeManager::instance().color("brls/background"));

    auto* header = new brls::Header();
    header->setTitle(title_);
    header->setSubtitle(subtitle_);
    listBox_->addView(header);

    if (buildRows_)
        buildRows_(listBox_);

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            playNavSfx();
            pageScroll(-1);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            playNavSfx();
            pageScroll(1);
            return true;
        });
}

void SettingsSectionView::pageScroll(int direction)
{
    const float viewH = scroller_->getHeight();
    const float page = viewH > 0.f ? viewH * 0.85f : 480.f;

    float offset = scroller_->getContentOffsetY();
    if (direction < 0)
        offset = std::max(0.f, offset - page);
    else
        offset += page;

    scroller_->setContentOffsetY(offset, true);
}

void SettingsSectionView::rebuildContent()
{
    listBox_->clearViews();

    auto* header = new brls::Header();
    header->setTitle(title_);
    header->setSubtitle(subtitle_);
    listBox_->addView(header);

    if (buildRows_)
        buildRows_(listBox_);

    applyThemeToSettingsPanel(this, listBox_);
}

void SettingsSectionView::refreshTheme()
{
    rebuildContent();
    brls::sync([this]() { focusFirstSettingsRow(listBox_); });
}

void SettingsSectionView::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        refreshTheme();
    }

    brls::Box::frame(ctx);
}

void SettingsSectionView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);

    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        rebuildContent();
    } else {
        applyThemeToSettingsPanel(this, listBox_);
    }

    if (onAppear_)
        onAppear_();

    brls::sync([this]() {
        scroller_->setContentOffsetY(0.f, false);
        focusFirstSettingsRow(listBox_);
    });
}

void SettingsSectionView::present(std::string title, std::string subtitle, BuildRowsFn buildRows,
                                  std::function<void()> onAppear)
{
    PushedActivity::push(
        new SettingsSectionView(std::move(title), std::move(subtitle), std::move(buildRows),
                                std::move(onAppear)));
}

} // namespace sf::ui
