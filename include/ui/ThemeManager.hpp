#pragma once

#include <nanovg.h>
#include <string>
#include <vector>

namespace sf::ui {

/** Loads color themes from data/theme/{name}/theme.xml and optional assets. */
class ThemeManager {
public:
    static ThemeManager& instance();

    /** Install bundled themes, load user selection, apply Borealis colors. */
    void initialize(const std::string& themeName);

    void setTheme(const std::string& name);
    const std::string& currentTheme() const { return currentTheme_; }
    bool isLightTheme() const { return variant_ == "light"; }

    std::vector<std::string> availableThemes() const;

    /** e.g. "Abyss  ◆ Dark" or "Horizon  ◇ Light" */
    std::string displayLabel(const std::string& folderName) const;
    std::string currentDisplayLabel() const { return displayLabel(currentTheme_); }

    /** Resolve a brls/* or nxstation/* color key from the active theme. */
    NVGcolor color(const std::string& key) const;
    bool hasColor(const std::string& key) const;

    /** Text color on a focused row — respects nxstation/selected_text or highlight contrast. */
    NVGcolor selectionTextColor(NVGcolor idleText) const;

    /** Migrate legacy folder names (blue → Abyss, etc.). */
    static std::string migrateThemeName(const std::string& name);

    std::string themePath() const;
    std::string systemBackgroundPath(const std::string& systemId) const;
    /** Theme file `{systemId}-list.jpg` (also .png/.webp) for the list-style preview card. */
    std::string systemListArtPath(const std::string& systemId) const;
    std::string systemPlaceholderPath() const;
    std::string boxPlaceholderPath() const;

    /** Optional theme assets (empty when missing). */
    std::string backgroundMusicPath() const;
    std::string screensaverImagePath() const;
    std::string sfxPath(const std::string& baseName) const;

    bool themeForcesListBrowser() const { return browserStyle_ == "list"; }

    void applyColors();
    /** Incremented when colors change; views repaint when generation differs. */
    void markUiRefresh();
    uint32_t uiGeneration() const { return uiGeneration_; }

private:
    ThemeManager() = default;
    void ensureBundledThemes();
    void seedThemeAudio(const std::string& name);
    bool loadThemeXml(const std::string& name);
    static bool copyFileIfMissing(const std::string& src, const std::string& dst);
    std::string findThemeAsset(const std::vector<const char*>& fileNames,
                               const std::vector<const char*>& subdirs) const;
    void applyThemeAssets();

    std::string currentTheme_ = "Vampire";
    std::string variant_ = "dark";
    std::string browserStyle_ = "carousel";
    std::string themeRoot_;
    uint32_t uiGeneration_ = 0;
    std::vector<std::pair<std::string, std::string>> colors_;
};

} // namespace sf::ui
