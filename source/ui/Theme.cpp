#include "ui/Theme.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/HaloLabel.hpp"
#include "ui/ZoomImage.hpp"

namespace sf::ui {

void applyModernTheme()
{
    registerZoomImageView();
    registerHaloLabelView();
    ThemeManager::instance().initialize("Vampire");
}

} // namespace sf::ui
