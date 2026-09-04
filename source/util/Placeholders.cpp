#include "util/Placeholders.hpp"
#include "ui/ThemeManager.hpp"
#include "util/FileSystem.hpp"

namespace sf {

std::string gameArtPlaceholderPath()
{
    if (std::string themed = sf::ui::ThemeManager::instance().boxPlaceholderPath(); !themed.empty())
        return themed;
    if (FileSystem::exists(paths::USER_GAME_ART_PLACEHOLDER))
        return paths::USER_GAME_ART_PLACEHOLDER;
    return {};
}

std::string systemArtPlaceholderPath()
{
    if (std::string themed = sf::ui::ThemeManager::instance().systemPlaceholderPath(); !themed.empty())
        return themed;
    if (FileSystem::exists(paths::USER_SYSTEM_ART_PLACEHOLDER))
        return paths::USER_SYSTEM_ART_PLACEHOLDER;
    return {};
}

const char* gameArtPlaceholderRes()
{
    return paths::BUNDLED_GAME_ART_PLACEHOLDER;
}

const char* systemArtPlaceholderRes()
{
    return paths::BUNDLED_SYSTEM_ART_PLACEHOLDER;
}

} // namespace sf
