#include "ui/SettingsTab.hpp"

#include "ui/PushedActivity.hpp"
#include "ui/SettingsHelpers.hpp"
#include "ui/SettingsSectionView.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "app/Config.hpp"

#include <functional>
#include <nanovg.h>

namespace sf::ui {

namespace {

void addSectionHubRow(brls::Box* listBox, const char* title, const char* subtitle,
                      SettingsSectionView::BuildRowsFn builder,
                      std::function<void()> onAppear = nullptr)
{
    auto* row = makeSettingsRow(title, subtitle);
    row->registerClickAction([title, subtitle, builder = std::move(builder),
                              onAppear = std::move(onAppear)](brls::View*) {
        SettingsSectionView::present(title, subtitle, builder, onAppear);
        return true;
    });
    listBox->addView(row);
}

} // namespace

SettingsTab::SettingsTab()
{
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    this->setFocusable(false);

    auto* header = new brls::Header();
    header->setTitle("Settings");
    header->setSubtitle("Loading…");
    listBox->addView(header);

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

void SettingsTab::pageScroll(int direction)
{
    const float viewH = scroller->getHeight();
    const float page = viewH > 0.f ? viewH * 0.85f : 480.f;

    float offset = scroller->getContentOffsetY();
    if (direction < 0)
        offset = std::max(0.f, offset - page);
    else
        offset += page;

    scroller->setContentOffsetY(offset, true);
}

void SettingsTab::onFocusRowChanged(brls::View* focused)
{
    if (!focused)
        return;

    brls::View* row = focused;
    while (row && row->getParent() != listBox)
        row = row->getParent();
    if (!row || row == lastFocusedRow_)
        return;

    lastFocusedRow_ = row;
    playNavSfx();
}

void SettingsTab::frame(brls::FrameContext* ctx)
{
    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        if (built_) {
            rebuild();
            brls::sync([this]() { focusFirstSettingsRow(listBox); });
        } else {
            applyThemeStyles();
        }
    }

    onFocusRowChanged(brls::Application::getCurrentFocus());
    brls::Box::frame(ctx);
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}

void SettingsTab::open()
{
    PushedActivity::push(new SettingsTab());
}

void SettingsTab::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;

    if (!built_) {
        themeGenApplied_ = ThemeManager::instance().uiGeneration();
        brls::sync([this, alive = alive_]() {
            if (!*alive || built_)
                return;
            rebuild();
            built_ = true;
            scroller->setContentOffsetY(0.f, false);
            focusFirstSettingsRow(listBox);
        });
        return;
    }

    const uint32_t themeGen = ThemeManager::instance().uiGeneration();
    if (themeGen != themeGenApplied_) {
        themeGenApplied_ = themeGen;
        rebuild();
        brls::sync([this]() {
            scroller->setContentOffsetY(0.f, false);
            focusFirstSettingsRow(listBox);
        });
    } else {
        applyThemeToSettingsPanel(this, listBox);
    }
}

void SettingsTab::willDisappear(bool resetState)
{
    *alive_ = false;
    Config::instance().saveUserSettings();
    SystemsTab::requestRefreshAfterSettings();
    brls::Box::willDisappear(resetState);
}

void SettingsTab::rebuild()
{
    listBox->clearViews();
    applyThemeToSettingsPanel(this, listBox);

    auto* header = new brls::Header();
    header->setTitle("Settings");
    header->setSubtitle("NXStation");
    listBox->addView(header);

    addSectionHubRow(listBox, "App & Launcher", "Forwarder, library scan, updates",
                     buildAppLauncherSettingsRows);
    addSectionHubRow(listBox, "Emulator and Systems", "RetroArch, ROMs, cores",
                     buildEmulatorSystemsSettingsRows);
    addSectionHubRow(listBox, "Appearance and Control", "Video, audio, theme, navigation",
                     buildAppearanceControlSettingsRows);
    addSectionHubRow(listBox, "Cloud Save", "Google Drive backup & restore",
                     buildCloudSaveSettingsRows, refreshCloudSaveSettingsSection);
    addSectionHubRow(listBox, "Scraper", "ScreenScraper website login",
                     buildScraperSettingsRows);
    addSectionHubRow(listBox, "Help and About", "Folders, logs, version",
                     buildHelpAboutSettingsRows);

    applyThemeToSettingsPanel(this, listBox);
}

void SettingsTab::applyThemeStyles()
{
    applyThemeToSettingsPanel(this, listBox);
}

} // namespace sf::ui
