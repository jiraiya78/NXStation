#include "ui/MainMenuOptionsView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "app/Config.hpp"
#include "util/ActionLog.hpp"

#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace sf::ui {

MainMenuOptionsView::MainMenuOptionsView(MainMenuCallback onSelect)
    : onSelect_(std::move(onSelect))
{
    stylePopupMenuPanel(this);

    auto* header = new brls::Header();
    header->setTitle("Main Menu");
    this->addView(header);

    auto menuScroller = makePopupMenuScroller(420.f);
    brls::ScrollingFrame* scroller = menuScroller.scroller;

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);

    auto addOption = [&](const char* title, MainMenuAction action) {
        auto* row = new brls::DetailCell();
        row->setText(title);
        stylePopupMenuRow(row);
        row->registerClickAction([this, action](brls::View*) {
            SF_LOG_ACTION("Systems/MainMenu");
            auto callback = onSelect_;
            if (dialog_) {
                dialog_->close([callback, action] {
                    if (callback)
                        callback(action);
                });
            } else if (callback) {
                callback(action);
            }
            return true;
        });
        list->addView(row);
    };

    addOption("Search", MainMenuAction::Search);
    addOption("Jump to System", MainMenuAction::JumpToSystem);
    addOption("Scan Games", MainMenuAction::ScanGames);
    if (Config::instance().cloudAutoSaveEnabled())
        addOption("Backup Saves to Cloud", MainMenuAction::SyncCloud);
    addOption("Personality Metrics", MainMenuAction::PersonalityMetrics);
    addOption("Playtime Analytics", MainMenuAction::PlaytimeAnalytics);
    addOption("Settings", MainMenuAction::Settings);

    scroller->setContentView(list);
    this->addView(menuScroller.clipBox);
    registerPopupPageActions(this, scroller);
}

void MainMenuOptionsView::present(MainMenuCallback onSelect)
{
    auto* menu = new MainMenuOptionsView(std::move(onSelect));
    auto* dialog = FocusedMenuDialog::present(menu);
    menu->dialog_ = dialog;
}

} // namespace sf::ui
