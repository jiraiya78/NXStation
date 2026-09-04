#pragma once

#include "util/GameArt.hpp"

#include <borealis/views/cells/cell_detail.hpp>
#include <borealis.hpp>

#include <string>

namespace sf::ui {

void styleSettingsCell(brls::DetailCell* cell);
void applyThemeToSettingsPanel(brls::View* panel, brls::Box* listBox);
brls::DetailCell* makeSettingsRow(const std::string& title, const std::string& detail = {});
void focusFirstSettingsRow(brls::Box* listBox);

std::string settingsVolumeLabel(float volume);
float nextSettingsVolumeStep(float current);
std::string settingsGameArtLabel(sf::GameArtMode mode);
std::string settingsScreensaverLabel(int seconds);
int nextSettingsScreensaverPreset(int current);
std::string settingsVideoPreviewDelayLabel(int seconds);
int nextSettingsVideoPreviewDelay(int current);
std::string settingsCloudLastBackupLabel();

} // namespace sf::ui
