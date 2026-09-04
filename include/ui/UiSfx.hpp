#pragma once

namespace sf::ui {

void initUiSfx();
bool isAudoutReady();

/** Reload navigation SFX from the active theme folder (falls back to romfs). */
void reloadThemeSfx();

void setNavSoundEnabled(bool enabled);
bool navSoundEnabled();

void setNavSoundVolume(float volume);
float navSoundVolume();

void playNavSfx();
void playConfirmSfx();
void playToggleSfx();
void playScrapeCompleteSfx();

} // namespace sf::ui
