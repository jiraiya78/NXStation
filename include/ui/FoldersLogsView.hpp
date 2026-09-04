#pragma once

#include <borealis.hpp>

namespace sf::ui {

/** Single scrollable list of every data/settings/log folder and file NXStation uses. */
class FoldersLogsView : public brls::Box {
public:
    FoldersLogsView();
    ~FoldersLogsView() override = default;

    static void present();
};

} // namespace sf::ui
