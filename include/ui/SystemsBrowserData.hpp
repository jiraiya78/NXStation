#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

namespace sf::ui::browser {

void requestSystemsDataRefresh();
bool consumeSystemsDataRefresh();

void rebuildSystemIds(std::vector<std::string>& out);
size_t initialSystemIndex(const std::vector<std::string>& systemIds);
std::string resolveSystemBackground(const std::string& systemId);
void applySystemPlaceholder(brls::Image* image);
std::string resolveSystemListArt(const std::string& systemId);
void applySystemListArt(brls::Image* image, const std::string& systemId);
std::string systemDescription(const std::string& systemId);
std::string systemDisplayName(const std::string& systemId);
std::string systemGameCountLabel(const std::string& systemId);

} // namespace sf::ui::browser
