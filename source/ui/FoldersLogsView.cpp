#include "ui/FoldersLogsView.hpp"
#include "app/Config.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <borealis/views/cells/cell_detail.hpp>

namespace sf::ui {

namespace {

brls::DetailCell* makeRow(const std::string& title, const std::string& detail)
{
    auto* row = new brls::DetailCell();
    row->setText(title);
    row->setDetailText(detail);
    row->setTextColor(ThemeManager::instance().color("nxstation/title_text"));
    row->setDetailTextColor(ThemeManager::instance().color("nxstation/detail_text"));
    row->setFocusable(false);
    return row;
}

brls::Header* makeHeader(const std::string& title, const std::string& subtitle)
{
    auto* header = new brls::Header();
    header->setTitle(title);
    header->setSubtitle(subtitle);
    return header;
}

} // namespace

FoldersLogsView::FoldersLogsView()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(16, 20, 16, 20);
    this->setBackgroundColor(ThemeManager::instance().color("brls/background"));

    auto* scroller = new brls::ScrollingFrame();
    scroller->setGrow(1.f);
    scroller->setScrollingIndicatorVisible(false);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setPadding(8, 12, 16, 12);

    auto* header = new brls::Header();
    header->setTitle("Folders & Logs");
    header->setSubtitle("Every path NXStation reads from or writes to");
    content->addView(header);

    content->addView(makeHeader("Data & Settings", {}));
    content->addView(makeRow("ROMs Root", Config::instance().effectiveRomsRoot()));
    content->addView(makeRow("Data Folder", paths::DATA_DIR));
    content->addView(makeRow("Settings Folder", paths::SETTINGS_DIR));
    content->addView(makeRow("Theme Folder", paths::THEME_DIR));

    content->addView(makeHeader("Logs", {}));
    content->addView(makeRow("Log Folder", paths::LOG_DIR));
    content->addView(makeRow("App Log", paths::LOG_PATH));
    content->addView(makeRow("Scrape Log", paths::SCRAPE_LOG_PATH));
    content->addView(makeRow("Cloud Log", paths::CLOUD_LOG_PATH));
    content->addView(makeRow("Crash Log", paths::CRASH_LOG_PATH));
    content->addView(makeRow("Boot Log", paths::BOOT_LOG_PATH));

    scroller->setContentView(content);
    this->addView(scroller);

    this->registerAction(
        "Back", brls::ControllerButton::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [scroller](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(std::max(0.f, scroller->getContentOffsetY() - page), true);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [scroller](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(scroller->getContentOffsetY() + page, true);
            return true;
        });
}

void FoldersLogsView::present()
{
    playConfirmSfx();
    PushedActivity::push(new FoldersLogsView());
}

} // namespace sf::ui
