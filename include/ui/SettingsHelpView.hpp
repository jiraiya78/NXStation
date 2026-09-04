#pragma once

#include <borealis.hpp>

namespace sf::ui {

/** Scrollable help text for data folder layout and user overrides. */
class SettingsHelpView : public brls::Box {
public:
    SettingsHelpView();
    ~SettingsHelpView() override = default;

    static void present();
};

} // namespace sf::ui
