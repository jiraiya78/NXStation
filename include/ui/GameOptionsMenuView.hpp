#pragma once

#include "app/Models.hpp"

#include <borealis.hpp>
#include <functional>
#include <string>

namespace sf::ui {

class FocusedMenuDialog;

enum class GameOptionsAction {
    GameManual,
    Search,
    JumpAlphabet,
    ShowMetadata,
    ScanGames,
    Scrape,
    Rename,
    Delete,
};

using GameOptionsCallback = std::function<void(GameOptionsAction action)>;

/** Y-button menu, shown as a modal dialog over the game list. */
class GameOptionsMenuView : public brls::Box {
public:
    GameOptionsMenuView(std::string systemId, const sf::GameItem& game, GameOptionsCallback onSelect);
    ~GameOptionsMenuView() override = default;

    /** Builds the menu wrapped in a Borealis dialog and opens it. */
    static void present(const std::string& systemId, const sf::GameItem& game,
                        GameOptionsCallback onSelect, std::function<void()> onDismiss = nullptr);

private:
    std::string systemId_;
    GameOptionsCallback onSelect_;
    FocusedMenuDialog* dialog_ = nullptr;
};

} // namespace sf::ui
