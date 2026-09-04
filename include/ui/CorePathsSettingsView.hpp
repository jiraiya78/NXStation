#pragma once

#include <borealis.hpp>

namespace sf::ui {

/** Per-system RetroArch core path browser (opened from Settings). */
class CorePathsSettingsView : public brls::Box {
public:
    CorePathsSettingsView();
    ~CorePathsSettingsView() override = default;

    static void present();

private:
    BRLS_BIND(brls::ScrollingFrame, scroller, "settings/scroller");
    BRLS_BIND(brls::Box, listBox, "settings/list");
};

} // namespace sf::ui
