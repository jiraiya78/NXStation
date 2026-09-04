#include "ui/SystemsBrowserData.hpp"

#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "ui/ThemeManager.hpp"
#include "util/FileSystem.hpp"
#include "util/Json.hpp"
#include "util/NavigationState.hpp"
#include "util/Paths.hpp"
#include "util/Placeholders.hpp"
#include "util/VirtualSystems.hpp"

#include <atomic>
#include <unordered_map>

namespace sf::ui::browser {

namespace {

std::atomic<bool> gSystemsDataRefreshRequested{false};

} // namespace

void requestSystemsDataRefresh()
{
    gSystemsDataRefreshRequested.store(true);
}

bool consumeSystemsDataRefresh()
{
    return gSystemsDataRefreshRequested.exchange(false);
}

std::string tryNameInDir(const std::string& dir, const std::string& name)
{
    for (const char* ext : {".jpg", ".jpeg", ".png", ".webp"}) {
        const std::string path = FileSystem::join(dir, name + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

void rebuildSystemIds(std::vector<std::string>& out)
{
    out.clear();

    if (!AppState::instance().gamesFor(kFavoritesSystemId).empty())
        out.push_back(kFavoritesSystemId);
    if (!AppState::instance().gamesFor(kLastPlayedSystemId).empty())
        out.push_back(kLastPlayedSystemId);

    for (const auto& sys : Config::instance().systems()) {
        if (Config::instance().hideEmptySystems()) {
            if (AppState::instance().gamesFor(sys.id).empty())
                continue;
        }
        out.push_back(sys.id);
    }
}

size_t initialSystemIndex(const std::vector<std::string>& systemIds)
{
    if (systemIds.empty())
        return 0;

    auto indexOf = [&](const std::string& id) -> size_t {
        for (size_t i = 0; i < systemIds.size(); ++i) {
            if (systemIds[i] == id)
                return i;
        }
        return systemIds.size();
    };

    const std::string remembered = NavigationState::lastSystem();
    if (!remembered.empty()) {
        if (size_t i = indexOf(remembered); i < systemIds.size())
            return i;
    }

    if (size_t i = indexOf(kLastPlayedSystemId); i < systemIds.size())
        return i;

    return 0;
}

std::string resolveSystemBackground(const std::string& systemId)
{
    if (std::string themed = ThemeManager::instance().systemBackgroundPath(systemId); !themed.empty())
        return themed;

    if (std::string custom = tryNameInDir(paths::BACKGROUNDS_DIR, systemId); !custom.empty())
        return custom;

    if (isVirtualSystemId(systemId)) {
        if (std::string custom =
                tryNameInDir(paths::BACKGROUNDS_DIR, virtualSystemBackgroundStem(systemId));
            !custom.empty())
            return custom;
    }

    if (std::string bundled = tryNameInDir("romfs:/img/background", systemId); !bundled.empty())
        return bundled;

    if (isVirtualSystemId(systemId)) {
        if (std::string bundled =
                tryNameInDir("romfs:/img/background", virtualSystemBackgroundStem(systemId));
            !bundled.empty())
            return bundled;
    }

    return {};
}

void applySystemPlaceholder(brls::Image* image)
{
    if (std::string custom = systemArtPlaceholderPath(); !custom.empty())
        image->setImageFromFile(custom);
    else
        image->setImageFromRes(systemArtPlaceholderRes());
}

std::string resolveSystemListArt(const std::string& systemId)
{
    if (std::string themed = ThemeManager::instance().systemListArtPath(systemId); !themed.empty())
        return themed;

    if (std::string custom = tryNameInDir(paths::BACKGROUNDS_DIR, systemId + "-list"); !custom.empty())
        return custom;

    const std::string bundled = "romfs:/img/systems/" + systemId + ".png";
    if (FileSystem::exists(bundled))
        return bundled;

    if (isVirtualSystemId(systemId)) {
        const std::string stem = virtualSystemBackgroundStem(systemId);
        if (std::string bundledVirtual = tryNameInDir("romfs:/img/background", stem); !bundledVirtual.empty())
            return bundledVirtual;
    }

    return {};
}

void applySystemListArt(brls::Image* image, const std::string& systemId)
{
    if (!image)
        return;

    const std::string path = resolveSystemListArt(systemId);
    if (!path.empty()) {
        if (path.rfind("romfs:/", 0) == 0)
            image->setImageFromRes(path.substr(7));
        else
            image->setImageFromFile(path);
        return;
    }

    if (std::string box = gameArtPlaceholderPath(); !box.empty())
        image->setImageFromFile(box);
    else
        image->setImageFromRes(gameArtPlaceholderRes());
}

namespace {

std::unordered_map<std::string, std::string> gSystemDescriptions;
bool gDescriptionsLoaded = false;

void loadSystemDescriptions()
{
    if (gDescriptionsLoaded)
        return;
    gDescriptionsLoaded = true;

    auto tryParse = [](const std::string& path) {
        const std::string data = FileSystem::readFile(path);
        if (data.empty())
            return false;
        try {
            const Json root = Json::parse(data);
            if (!root.is_object())
                return false;
            for (const auto& [key, value] : root.items()) {
                if (value.is_string())
                    gSystemDescriptions[key] = value.get<std::string>();
            }
            return !gSystemDescriptions.empty();
        } catch (...) {
            return false;
        }
    };

    if (FileSystem::exists(paths::SYSTEM_DESCRIPTIONS_PATH))
        tryParse(paths::SYSTEM_DESCRIPTIONS_PATH);
    if (gSystemDescriptions.empty())
        tryParse(paths::SYSTEM_DESCRIPTIONS_FALLBACK);
}

} // namespace

std::string systemDescription(const std::string& systemId)
{
    loadSystemDescriptions();
    auto it = gSystemDescriptions.find(systemId);
    if (it != gSystemDescriptions.end() && !it->second.empty())
        return it->second;
    return "No description yet. Edit settings/system_descriptions.json to add one.";
}

std::string systemDisplayName(const std::string& systemId)
{
    if (isVirtualSystemId(systemId))
        return virtualSystemDisplayName(systemId);
    if (const SystemConfig* sys = Config::instance().findSystem(systemId))
        return sys->name;
    return systemId;
}

std::string systemGameCountLabel(const std::string& systemId)
{
    const size_t count = AppState::instance().gamesFor(systemId).size();
    return std::to_string(count) + " games";
}

} // namespace sf::ui::browser
