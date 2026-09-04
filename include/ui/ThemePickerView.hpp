#pragma once

#include <functional>
#include <string>

namespace sf::ui {

/** Popup list to pick an installed theme from data/theme. */
class ThemePickerView {
public:
    using Callback = std::function<void(std::string themeFolder)>;

    static void present(Callback onSelect);
};

} // namespace sf::ui
