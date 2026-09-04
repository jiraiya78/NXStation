#include "ui/ThemeManager.hpp"

#include "media/ThemeBgmPlayer.hpp"
#include "ui/UiSfx.hpp"

#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <borealis.hpp>
#include <nanovg.h>
#include <tinyxml2.h>

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace sf::ui {

namespace {

NVGcolor parseHexColor(const std::string& hex)
{
    std::string s = hex;
    if (!s.empty() && s[0] == '#')
        s.erase(s.begin());

    unsigned int r = 0, g = 0, b = 0, a = 255;
    if (s.size() == 6)
        std::sscanf(s.c_str(), "%2x%2x%2x", &r, &g, &b);
    else if (s.size() == 8)
        std::sscanf(s.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
    return nvgRGBA(static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                   static_cast<unsigned char>(b), static_cast<unsigned char>(a));
}

void tuneTheme(brls::Theme& theme, const std::vector<std::pair<std::string, std::string>>& colors)
{
    for (const auto& [key, value] : colors)
        theme.addColor(key, parseHexColor(value));
}

const char* bundledThemeNames[] = {
    "Vampire",  "Abyss",    "Sunfire",  "Carnage",  "Glacial",  "Mirage",
    "Cyberpunk", "Warlock", "Toxic",    "Horizon",  "Solstice", "Prism",
    "Iceberg",  "Sunshine", "Flamingo", "Clay",     "Meadow",
};

const std::unordered_map<std::string, std::string> kLegacyThemeNames = {
    {"blue", "Abyss"},     {"orange", "Sunfire"}, {"red", "Carnage"},
    {"cyan", "Glacial"},   {"mustard", "Mirage"}, {"pink", "Cyberpunk"},
    {"maroon", "Warlock"}, {"green", "Toxic"},
};

bool readThemeMeta(const std::string& folderName, std::string* outName, std::string* outVariant)
{
    const std::string xmlPath = FileSystem::join(paths::THEME_DIR, folderName) + "/theme.xml";
    std::string xml = FileSystem::readFile(xmlPath);
    if (xml.empty())
        return false;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
        return false;

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
        return false;

    if (outName) {
        const char* name = root->Attribute("name");
        *outName = name ? name : folderName;
    }
    if (outVariant) {
        const char* variant = root->Attribute("variant");
        *outVariant = variant ? variant : "dark";
    }
    return true;
}

int themeSortRank(const std::string& name, const std::string& variant)
{
    if (name == "Vampire")
        return 0;
    if (variant == "dark")
        return 1;
    return 2;
}

} // namespace

ThemeManager& ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

std::string ThemeManager::migrateThemeName(const std::string& name)
{
    auto it = kLegacyThemeNames.find(name);
    if (it != kLegacyThemeNames.end())
        return it->second;
    return name;
}

bool ThemeManager::copyFileIfMissing(const std::string& src, const std::string& dst)
{
    if (FileSystem::exists(dst))
        return true;
    std::string data = FileSystem::readFile(src);
    if (data.empty())
        return false;
    return FileSystem::writeFile(dst, data);
}

void ThemeManager::seedThemeAudio(const std::string& name)
{
    const std::string destDir = FileSystem::join(paths::THEME_DIR, name);
    FileSystem::createDirectories(destDir);

#ifdef __SWITCH__
    const std::string srcDir =
        std::string("romfs:/") + paths::BUNDLED_THEMES_RES + "/" + name;
#else
    const std::string srcDir = FileSystem::join("resources/themes", name);
#endif
    for (const auto& entry : FileSystem::listDirectory(srcDir)) {
        if (entry.isDirectory)
            continue;
        const std::string lower = FileSystem::toLower(entry.name);
        if (lower.rfind("bgm.", 0) != 0 && lower.rfind("music.", 0) != 0)
            continue;
        copyFileIfMissing(entry.path, FileSystem::join(destDir, entry.name));
    }
}

void ThemeManager::ensureBundledThemes()
{
    FileSystem::createDirectories(paths::THEME_DIR);
    FileSystem::createDirectories(FileSystem::join(paths::APP_ROOT, "sample"));

#ifdef __SWITCH__
    copyFileIfMissing("romfs:/sample/bgm.mp3",
                      FileSystem::join(FileSystem::join(paths::APP_ROOT, "sample"), "bgm.mp3"));
#endif

    for (const char* name : bundledThemeNames) {
        const std::string destDir = FileSystem::join(paths::THEME_DIR, name);
        FileSystem::createDirectories(destDir);
        seedThemeAudio(name);

        const std::string destXml = FileSystem::join(destDir, "theme.xml");
        if (FileSystem::exists(destXml))
            continue;

#ifdef __SWITCH__
        const std::string srcXml =
            std::string("romfs:/") + paths::BUNDLED_THEMES_RES + "/" + name + "/theme.xml";
#else
        const std::string srcXml = FileSystem::join("resources/themes", std::string(name) + "/theme.xml");
#endif
        if (copyFileIfMissing(srcXml, destXml))
            SF_LOG_I("Theme", "Installed bundled theme: %s", name);
        else
            SF_LOG_W("Theme", "Could not install theme: %s", name);
    }
}

bool ThemeManager::loadThemeXml(const std::string& name)
{
    colors_.clear();
    variant_ = "dark";
    themeRoot_ = FileSystem::join(paths::THEME_DIR, name);

    const std::string xmlPath = FileSystem::join(themeRoot_, "theme.xml");
    std::string xml = FileSystem::readFile(xmlPath);
    if (xml.empty()) {
        SF_LOG_W("Theme", "Missing theme.xml for %s", name.c_str());
        return false;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        SF_LOG_W("Theme", "Parse error in %s", xmlPath.c_str());
        return false;
    }

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
        return false;

    if (const char* variant = root->Attribute("variant"))
        variant_ = variant;
    if (const char* browser = root->Attribute("browser"))
        browserStyle_ = browser;
    else
        browserStyle_ = "carousel";

    const tinyxml2::XMLElement* colors = root->FirstChildElement("colors");
    if (!colors)
        return false;

    for (const tinyxml2::XMLElement* color = colors->FirstChildElement("color"); color;
         color = color->NextSiblingElement("color")) {
        const char* key = color->Attribute("key");
        const char* value = color->Attribute("value");
        if (key && value)
            colors_.emplace_back(key, value);
    }

    SF_LOG_I("Theme", "Loaded %zu colors from %s (%s)", colors_.size(), name.c_str(),
             variant_.c_str());
    return !colors_.empty();
}

std::string ThemeManager::findThemeAsset(const std::vector<const char*>& fileNames,
                                         const std::vector<const char*>& subdirs) const
{
    if (themeRoot_.empty())
        return {};

    for (const char* subdir : subdirs) {
        for (const char* name : fileNames) {
            const std::string path =
                subdir && subdir[0] != '\0'
                    ? FileSystem::join(FileSystem::join(themeRoot_, subdir), name)
                    : FileSystem::join(themeRoot_, name);
            if (FileSystem::exists(path))
                return path;
        }
    }
    return {};
}

std::string ThemeManager::backgroundMusicPath() const
{
    static const std::vector<const char*> kNames = {"bgm.mp3", "bgm.ogg", "bgm.wav",
                                                    "music.mp3", "music.ogg", "music.wav"};
    static const std::vector<const char*> kDirs = {"", "audio", "music"};
    if (std::string found = findThemeAsset(kNames, kDirs); !found.empty())
        return found;

    auto matchStem = [](const std::string& filename) {
        const std::string lower = FileSystem::toLower(filename);
        const bool nameOk = lower.rfind("bgm.", 0) == 0 || lower.rfind("music.", 0) == 0;
        if (!nameOk)
            return false;
        return lower.ends_with(".mp3") || lower.ends_with(".ogg") || lower.ends_with(".wav");
    };

    for (const char* subdir : kDirs) {
        const std::string dir =
            subdir[0] != '\0' ? FileSystem::join(themeRoot_, subdir) : themeRoot_;
        for (const auto& entry : FileSystem::listDirectory(dir)) {
            if (entry.isDirectory)
                continue;
            if (matchStem(entry.name))
                return entry.path;
        }
    }

    const std::string sampleDir = FileSystem::join(paths::APP_ROOT, "sample");
    for (const auto& entry : FileSystem::listDirectory(sampleDir)) {
        if (entry.isDirectory)
            continue;
        if (matchStem(entry.name))
            return entry.path;
    }

#ifdef __SWITCH__
    if (!currentTheme_.empty()) {
        static const std::vector<const char*> kRomfsNames = {
            "bgm.mp3", "bgm.ogg", "bgm.wav", "music.mp3", "music.ogg", "music.wav"};
        for (const char* name : kRomfsNames) {
            const std::string romfsBgm = std::string("romfs:/") + paths::BUNDLED_THEMES_RES + "/"
                                         + currentTheme_ + "/" + name;
            if (FileSystem::exists(romfsBgm))
                return romfsBgm;
        }
        const std::string romfsSample = "romfs:/sample/bgm.mp3";
        if (FileSystem::exists(romfsSample))
            return romfsSample;
    }
#endif
    return {};
}

std::string ThemeManager::screensaverImagePath() const
{
    static const std::vector<const char*> kLightNames = {
        "screensaver-light.jpg", "screensaver-light.jpeg", "screensaver-light.png",
        "screensaver-light.webp"};
    static const std::vector<const char*> kDarkNames = {"screensaver.jpg", "screensaver.jpeg",
                                                        "screensaver.png", "screensaver.webp"};
    static const std::vector<const char*> kDirs = {"", "images"};

    if (isLightTheme()) {
        if (std::string found = findThemeAsset(kLightNames, kDirs); !found.empty())
            return found;
    }
    if (std::string found = findThemeAsset(kDarkNames, kDirs); !found.empty())
        return found;

    const char* bundledLight[] = {"romfs:/img/screensaver-light.jpg",
                                  "romfs:/img/screensaver-light.png"};
    const char* bundledDark[] = {"romfs:/img/screensaver.jpg", "romfs:/img/screensaver.png"};
    if (isLightTheme()) {
        for (const char* path : bundledLight) {
            if (FileSystem::exists(path))
                return path;
        }
    }
    for (const char* path : bundledDark) {
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string ThemeManager::sfxPath(const std::string& baseName) const
{
    const std::string file = baseName + ".wav";
    static const std::vector<const char*> kDirs = {"sfx", ""};
    for (const char* subdir : kDirs) {
        const std::string path =
            subdir[0] != '\0' ? FileSystem::join(FileSystem::join(themeRoot_, subdir), file)
                              : FileSystem::join(themeRoot_, file);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

void ThemeManager::applyThemeAssets()
{
    const std::string bgm = backgroundMusicPath();
    if (bgm.empty())
        SF_LOG_W("BGM", "No theme music in %s or %s/sample (drop bgm.mp3 here)",
                 themeRoot_.c_str(), paths::APP_ROOT);
    else
        SF_LOG_I("BGM", "Using %s", bgm.c_str());
    audio::ThemeBgmPlayer::instance().reload(bgm);
    reloadThemeSfx();
}

void ThemeManager::initialize(const std::string& themeName)
{
    ensureBundledThemes();

    currentTheme_ = migrateThemeName(themeName.empty() ? "Vampire" : themeName);
    if (!loadThemeXml(currentTheme_)) {
        currentTheme_ = "Vampire";
        loadThemeXml(currentTheme_);
    }

    applyColors();
    applyThemeAssets();
}

std::vector<std::string> ThemeManager::availableThemes() const
{
    std::vector<std::string> names;
    for (const char* bundled : bundledThemeNames) {
        const std::string xml = FileSystem::join(paths::THEME_DIR, bundled) + "/theme.xml";
        if (FileSystem::exists(xml))
            names.emplace_back(bundled);
    }

    auto entries = FileSystem::listDirectory(paths::THEME_DIR);
    for (const auto& entry : entries) {
        if (!entry.isDirectory)
            continue;
        const std::string xml = FileSystem::join(entry.path, "theme.xml");
        if (!FileSystem::exists(xml))
            continue;
        if (std::find(names.begin(), names.end(), entry.name) == names.end())
            names.push_back(entry.name);
    }

    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        std::string va, vb, na, nb;
        readThemeMeta(a, &na, &va);
        readThemeMeta(b, &nb, &vb);
        const int ra = themeSortRank(na.empty() ? a : na, va);
        const int rb = themeSortRank(nb.empty() ? b : nb, vb);
        if (ra != rb)
            return ra < rb;
        return (na.empty() ? a : na) < (nb.empty() ? b : nb);
    });
    return names;
}

std::string ThemeManager::displayLabel(const std::string& folderName) const
{
    std::string name, variant;
    if (!readThemeMeta(folderName, &name, &variant))
        name = folderName;

    const char* identity = (variant == "light") ? "◇ Light" : "◆ Dark";
    return name + "  " + identity;
}

void ThemeManager::setTheme(const std::string& name)
{
    const std::string resolved = migrateThemeName(name);
    if (resolved == currentTheme_)
        return;
    if (!loadThemeXml(resolved)) {
        SF_LOG_W("Theme", "Failed to switch to theme %s", resolved.c_str());
        return;
    }
    currentTheme_ = resolved;
    applyColors();
    applyThemeAssets();
    markUiRefresh();
}

void ThemeManager::markUiRefresh()
{
    ++uiGeneration_;
}

NVGcolor ThemeManager::color(const std::string& key) const
{
    for (const auto& [k, value] : colors_) {
        if (k == key)
            return parseHexColor(value);
    }
    try {
        return brls::Application::getTheme()[key];
    } catch (...) {
        return nvgRGBA(200, 200, 200, 255);
    }
}

bool ThemeManager::hasColor(const std::string& key) const
{
    for (const auto& [k, value] : colors_) {
        if (k == key)
            return true;
    }
    return brls::Application::getTheme().hasColor(key);
}

namespace {

float colorLuminance(NVGcolor c)
{
    return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
}

} // namespace

NVGcolor ThemeManager::selectionTextColor(NVGcolor idleText) const
{
    if (hasColor("nxstation/selected_text"))
        return color("nxstation/selected_text");

    const NVGcolor highlightBg = color("brls/highlight/background");
    if (colorLuminance(highlightBg) < 0.45f)
        return nvgRGBA(255, 255, 255, 255);
    return idleText;
}

std::string ThemeManager::themePath() const
{
    return themeRoot_;
}

std::string ThemeManager::systemBackgroundPath(const std::string& systemId) const
{
    const std::string dir = FileSystem::join(themeRoot_, "backgrounds");
    for (const char* ext : {".jpg", ".jpeg", ".png", ".webp"}) {
        const std::string path = FileSystem::join(dir, systemId + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string ThemeManager::systemListArtPath(const std::string& systemId) const
{
    const std::string stem = systemId + "-list";
    for (const char* subdir : {"", "backgrounds", "images"}) {
        const std::string dir =
            subdir[0] != '\0' ? FileSystem::join(themeRoot_, subdir) : themeRoot_;
        for (const char* ext : {".jpg", ".jpeg", ".png", ".webp"}) {
            const std::string path = FileSystem::join(dir, stem + ext);
            if (FileSystem::exists(path))
                return path;
        }
    }
    return {};
}

std::string ThemeManager::systemPlaceholderPath() const
{
    for (const char* name : {"placeholder.jpg", "nxstation.jpg"}) {
        const std::string path = FileSystem::join(themeRoot_, name);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string ThemeManager::boxPlaceholderPath() const
{
    for (const char* name : {"placeholder_box.png", "nxstation_box.png"}) {
        const std::string path = FileSystem::join(themeRoot_, name);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

void ThemeManager::applyColors()
{
    const auto variant = variant_ == "light" ? brls::ThemeVariant::LIGHT : brls::ThemeVariant::DARK;
    brls::Application::getPlatform()->setThemeVariant(variant);
    tuneTheme(brls::Theme::getLightTheme(), colors_);
    tuneTheme(brls::Theme::getDarkTheme(), colors_);

    brls::getStyle().addMetric("brls/sidebar/item_height", 72.0f);
    brls::getStyle().addMetric("brls/sidebar/item_font_size", 23.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_left", 32.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_right", 28.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_top", 20.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_bottom", 20.0f);
    brls::getStyle().addMetric("brls/listitem/descriptionIndent", 22.0f);
    markUiRefresh();
}

} // namespace sf::ui
