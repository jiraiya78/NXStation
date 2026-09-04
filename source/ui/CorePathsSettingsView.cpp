#include "ui/CorePathsSettingsView.hpp"
#include "app/Config.hpp"
#include "ui/FileBrowserView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/ActionLog.hpp"

#include <borealis/views/cells/cell_detail.hpp>

namespace sf::ui {

namespace {

void styleCell(brls::DetailCell* cell)
{
    cell->setTextColor(ThemeManager::instance().color("nxstation/title_text"));
    cell->setDetailTextColor(ThemeManager::instance().color("nxstation/detail_text"));
}

} // namespace

CorePathsSettingsView::CorePathsSettingsView()
{
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    this->setBackgroundColor(ThemeManager::instance().color("brls/background"));

    auto* header = new brls::Header();
    header->setTitle("Core Paths");
    header->setSubtitle("Tap a system to browse for its RetroArch core .nro");
    listBox->addView(header);

    for (const auto& sys : Config::instance().systems()) {
        std::string core = Config::instance().coreFor(sys.id);
        std::string shortCore = core;
        if (shortCore.size() > 44)
            shortCore = "…" + shortCore.substr(shortCore.size() - 42);

        auto* row = new brls::DetailCell();
        row->setText(sys.name);
        row->setDetailText(shortCore);
        styleCell(row);
        std::string systemId = sys.id;
        std::string systemName = sys.name;
        row->registerClickAction([row, systemId, systemName](brls::View*) {
            SF_LOG_ACTION("Settings/BrowseCore");
            std::string current = Config::instance().coreFor(systemId);
            FileBrowserView::present(
                current, {".nro"},
                [row, systemId](std::string path) {
                    Config::instance().setSystemCore(systemId, path);
                    Config::instance().saveUserCores();
                    std::string shown = path;
                    if (shown.size() > 44)
                        shown = "…" + shown.substr(shown.size() - 42);
                    row->setDetailText(shown);
                    brls::Application::notify("Core path saved");
                },
                systemName + " — Select Core");
            return true;
        });
        listBox->addView(row);
    }

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            playNavSfx();
            const float viewH = scroller->getHeight();
            const float page = viewH > 0.f ? viewH * 0.85f : 480.f;
            scroller->setContentOffsetY(std::max(0.f, scroller->getContentOffsetY() - page), true);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            playNavSfx();
            const float viewH = scroller->getHeight();
            const float page = viewH > 0.f ? viewH * 0.85f : 480.f;
            scroller->setContentOffsetY(scroller->getContentOffsetY() + page, true);
            return true;
        });
}

void CorePathsSettingsView::present()
{
    PushedActivity::push(new CorePathsSettingsView());
}

} // namespace sf::ui
