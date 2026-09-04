#pragma once

#include <borealis.hpp>
#include <functional>

namespace sf::ui {

enum class MainMenuAction {
    Search,
    JumpToSystem,
    ScanGames,
    SyncCloud,
    PersonalityMetrics,
    PlaytimeAnalytics,
    Settings,
};

using MainMenuCallback = std::function<void(MainMenuAction action)>;

/** Y-button menu on the systems carousel. */
class MainMenuOptionsView : public brls::Box {
public:
    explicit MainMenuOptionsView(MainMenuCallback onSelect);
    ~MainMenuOptionsView() override = default;

    static void present(MainMenuCallback onSelect);

private:
    MainMenuCallback onSelect_;
    class FocusedMenuDialog* dialog_ = nullptr;
};

} // namespace sf::ui
