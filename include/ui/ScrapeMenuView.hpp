#pragma once

#include "scraper/ScrapeTypes.hpp"

#include <borealis.hpp>
#include <functional>
#include <string>

namespace sf::ui {

class FocusedMenuDialog;

using ScrapeMenuCallback = std::function<void(ScrapeMode mode)>;

class ScrapeMenuView : public brls::Box {
public:
    ScrapeMenuView(std::string systemId, ScrapeMenuCallback onSelect);
    ~ScrapeMenuView() override = default;

    /** Builds the menu wrapped in a Borealis dialog and opens it. */
    static void present(const std::string& systemId, ScrapeMenuCallback onSelect,
                        std::function<void()> onDismiss = nullptr);

private:
    std::string systemId_;
    ScrapeMenuCallback onSelect_;
    FocusedMenuDialog* dialog_ = nullptr;
};

} // namespace sf::ui
