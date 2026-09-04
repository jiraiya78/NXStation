#include "ui/GameSearch.hpp"
#include "ui/GameListView.hpp"
#include "ui/PushedActivity.hpp"
#include "app/AppState.hpp"
#include "app/Config.hpp"
#include "util/ActionLog.hpp"
#include "util/Logger.hpp"
#include "util/Utf8.hpp"
#include "util/VirtualSystems.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <unordered_set>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf::ui::GameSearch {

namespace {

constexpr size_t kMaxResults = 400;

bool readQuery(std::string& out)
{
#ifdef __SWITCH__
    SwkbdConfig config;
    if (R_FAILED(swkbdCreate(&config, 0)))
        return false;

    swkbdConfigMakePresetDefault(&config);
    swkbdConfigSetHeaderText(&config, "Search games");
    swkbdConfigSetSubText(&config, "Matches any part of the title");
    swkbdConfigSetStringLenMax(&config, 64);
    swkbdConfigSetInitialText(&config, "");

    char buffer[256] = {};
    const Result rc = swkbdShow(&config, buffer, sizeof(buffer));
    swkbdClose(&config);

    if (R_FAILED(rc))
        return false;

    out.assign(buffer);
    return true;
#else
    (void)out;
    brls::Application::notify("Search needs the Switch keyboard");
    return false;
#endif
}

std::string trimmed(std::string s)
{
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::vector<sf::GameItem> collectMatches(const std::string& systemId, const std::string& query)
{
    std::vector<std::string> scope;
    if (!systemId.empty() && !isVirtualSystemId(systemId)) {
        scope.push_back(systemId);
    } else {
        for (const auto& sys : Config::instance().systems())
            scope.push_back(sys.id);
    }

    std::vector<sf::GameItem> matches;
    std::unordered_set<std::string> seen;

    for (const std::string& id : scope) {
        for (const auto& game : AppState::instance().gamesFor(id)) {
            if (!Utf8::containsFolded(game.displayName, query))
                continue;
            if (!seen.insert(game.path).second)
                continue;

            sf::GameItem copy = game;
            if (copy.systemId.empty())
                copy.systemId = id;
            matches.push_back(std::move(copy));
        }
    }

    std::sort(matches.begin(), matches.end(),
              [](const sf::GameItem& a, const sf::GameItem& b) {
                  return Utf8::compareTitles(a.displayName, b.displayName);
              });

    if (matches.size() > kMaxResults)
        matches.resize(kMaxResults);

    return matches;
}

} // namespace

void prompt(const std::string& systemId)
{
    std::string raw;
    if (!readQuery(raw))
        return;

    const std::string query = trimmed(std::move(raw));
    if (query.empty())
        return;

    SF_LOG_ACTION("Search/Run");

    auto matches = collectMatches(systemId, query);
    if (matches.empty()) {
        brls::Application::notify("No games match \"" + query + "\"");
        return;
    }

    const size_t count = matches.size();
    const std::string subtitle =
        std::to_string(count) + (count == 1 ? " match" : " matches");

    SF_LOG_I("UI", "Search '%s' -> %zu results", query.c_str(), count);

    auto* view = new GameListView("Search: " + query, std::move(matches), subtitle);
    brls::sync([view]() { PushedActivity::push(view); });
}

} // namespace sf::ui::GameSearch
