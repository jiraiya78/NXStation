#include "ui/GameOptionsMenuView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "app/Config.hpp"
#include "media/ManualPages.hpp"
#include "util/ActionLog.hpp"
#include "util/FileSystem.hpp"

#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace sf::ui {

GameOptionsMenuView::GameOptionsMenuView(std::string systemId, const sf::GameItem& game,
                                         GameOptionsCallback onSelect)
    : systemId_(std::move(systemId))
    , onSelect_(std::move(onSelect))
{
    stylePopupMenuPanel(this);

    const SystemConfig* sys = Config::instance().findSystem(systemId_);
    auto* header = new brls::Header();
    header->setTitle(game.displayName);
    header->setSubtitle(sys ? sys->name : systemId_);
    this->addView(header);

    auto menuScroller = makePopupMenuScroller(420.f);
    brls::ScrollingFrame* scroller = menuScroller.scroller;

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);

    auto addOption = [&](const char* title, GameOptionsAction action) {
        auto* row = new brls::DetailCell();
        row->setText(title);
        stylePopupMenuRow(row);
        row->registerClickAction([this, action](brls::View*) {
            SF_LOG_ACTION("GameList/Options");
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

    const std::string romStem = FileSystem::stemOf(game.path);
    if (sys && ManualPages::hasManualEntry(sys->path, romStem, game.meta))
        addOption("◆ Game Manual", GameOptionsAction::GameManual);

    addOption("Search", GameOptionsAction::Search);
    addOption("Jump to Letter / Number", GameOptionsAction::JumpAlphabet);
    addOption("Show Metadata", GameOptionsAction::ShowMetadata);
    addOption("Scrape", GameOptionsAction::Scrape);
    addOption("Rename ROM", GameOptionsAction::Rename);
    addOption("Delete ROM", GameOptionsAction::Delete);
    addOption("Scan Games", GameOptionsAction::ScanGames);

    scroller->setContentView(list);
    this->addView(menuScroller.clipBox);
    registerPopupPageActions(this, scroller);
}

void GameOptionsMenuView::present(const std::string& systemId, const sf::GameItem& game,
                                  GameOptionsCallback onSelect, std::function<void()> onDismiss)
{
    auto* menu = new GameOptionsMenuView(systemId, game, std::move(onSelect));
    auto* dialog = FocusedMenuDialog::present(menu, std::move(onDismiss));
    menu->dialog_ = dialog;
}

} // namespace sf::ui
