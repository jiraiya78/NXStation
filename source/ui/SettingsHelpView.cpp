#include "ui/SettingsHelpView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <nanovg.h>

namespace sf::ui {

namespace {

brls::Label* makeSection(const std::string& title, const std::string& body)
{
    auto* label = new brls::Label();
    label->setText(title + "\n" + body);
    label->setFontSize(20);
    label->setLineHeight(1.45f);
    label->setSingleLine(false);
    label->setTextColor(ThemeManager::instance().color("nxstation/body_text"));
    return label;
}

} // namespace

SettingsHelpView::SettingsHelpView()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(16, 20, 16, 20);
    this->setBackgroundColor(ThemeManager::instance().color("brls/background"));

    auto* scroller = new brls::ScrollingFrame();
    scroller->setGrow(1.f);
    scroller->setScrollingIndicatorVisible(false);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setPadding(8, 12, 16, 12);

    auto* header = new brls::Header();
    header->setTitle("Data Folders & Overrides");
    header->setSubtitle("Place files on your SD card under NXStation paths");
    content->addView(header);

    const std::string res = paths::USER_RESOURCES_DIR;
    const std::string bg = paths::BACKGROUNDS_DIR;
    const std::string data = paths::DATA_DIR;

    content->addView(makeSection(
        "Themes",
        "Color themes live in:\n"
        "  " +
            std::string(paths::THEME_DIR) +
            "/{theme_name}/theme.xml\n"
            "Optional assets in the same folder:\n"
            "  placeholder.jpg — system carousel fallback\n"
            "  placeholder_box.png — missing game box art\n"
            "  backgrounds/{systemId}.jpg — per-system wallpaper\n"
            "Change theme in Settings → Theme. Default is Vampire."));

    content->addView(makeSection(
        "Logs",
        "All logs are stored under:\n"
        "  " +
            std::string(paths::LOG_DIR) +
            "/\n"
            "  NXStation.log — app log\n"
            "  scrape.log — scraping sessions\n"
            "  crash.log — crash dumps\n"
            "  boot.log — early startup milestones"));

    content->addView(makeSection(
        "Settings files",
        "JSON configuration is stored under:\n"
        "  " +
            std::string(paths::SETTINGS_DIR) +
            "/\n"
            "  roms_config.json — master system list (add systems here)\n"
            "  user_cores.json — core path overrides only (Settings UI)\n"
            "  user_screenscraper.json — ScreenScraper website login\n"
            "  user_settings.json — theme, audio, video prefs\n"
            "  user_favorites.json — starred games\n"
            "  navigation_state.json — resume position"));

    content->addView(makeSection(
        "Adding a system",
        "Only roms_config.json defines systems. user_cores.json cannot add new ones.\n\n"
        "1. Edit " +
            std::string(paths::CONFIG_PATH) +
            "\n"
        "2. Add an entry to the systems array:\n"
        "   id — folder name under sdmc:/roms/ (e.g. saturn)\n"
        "   name — label shown in the UI\n"
        "   path — ROM folder (e.g. sdmc:/roms/saturn)\n"
        "   core — RetroArch core .nro path\n"
        "   extensions — file types to scan (.iso, .zip, …)\n"
        "   ssSystemId — ScreenScraper numeric system id (for scraping)\n"
        "3. Create the ROM folder and add games.\n"
        "4. Scan Games / Rescan Libraries (reloads roms_config.json) or restart NXStation.\n\n"
        "With Hide Empty Systems On, the system appears after it has at least one ROM scanned.\n\n"
        "ssSystemId values: open the ScreenScraper system list (logged in):\n"
        "  https://www.screenscraper.fr/index.php?action=systemesListe\n"
        "Use the numeric id shown for your platform (same as API systemeid).\n"
        "Bundled defaults are in settings/roms_config.json after first launch.\n\n"
        "Optional: carousel wallpaper at data/backgrounds/{id}.jpg"));

    content->addView(makeSection(
        "Placeholder images",
        "Override bundled placeholders by adding files to:\n"
        "  " +
            res +
            "/nxstation.jpg\n"
            "    → system carousel when no system background exists\n"
            "  " +
            res +
            "/nxstation_box.png\n"
            "    → missing game box art in the game list"));

    content->addView(makeSection(
        "System backgrounds",
        "Per-system carousel wallpaper (matched by system id):\n"
        "  " +
            bg +
            "/{systemId}.jpg\n"
            "Also supported: .jpeg, .png, .webp\n"
            "Example: " +
            bg + "/snes.jpg"));

    content->addView(makeSection(
        "Game box art (ES-DE layout)",
        "Inside each ROM folder:\n"
        "  sdmc:/roms/{systemId}/images/{stem}-image.jpg\n"
        "  sdmc:/roms/{systemId}/images/{stem}-thumb.jpg\n"
        "{stem} = ROM filename without extension"));

    content->addView(makeSection(
        "Game box art (NXStation layout)",
        "Central artwork cache:\n"
        "  " +
            data +
            "/artwork/{systemId}/{stem}_box.jpg\n"
            "  " +
            data + "/artwork/{systemId}/{stem}_logo.jpg"));

    content->addView(makeSection(
        "Video previews",
        "  sdmc:/roms/{systemId}/videos/{stem}-video.mp4\n"
        "  " +
            data + "/video/{systemId}/{stem}.mp4"));

    content->addView(makeSection(
        "gamelist.xml",
        "Per-system metadata (EmulationStation format):\n"
        "  sdmc:/roms/{systemId}/gamelist.xml\n"
        "Entries can set name, desc, image, thumbnail, and video paths.\n"
        "Rescan libraries in Settings after adding files."));

    content->addView(makeSection(
        "Credits",
        "Theme music (bgm.mp3) by Vlad Krotov from Pixabay.\n"
        "NXStation also uses RetroArch, ScreenScraper, and Borealis."));

    scroller->setContentView(content);
    this->addView(scroller);

    this->registerAction(
        "Back", brls::ControllerButton::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [scroller](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(std::max(0.f, scroller->getContentOffsetY() - page), true);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [scroller](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(scroller->getContentOffsetY() + page, true);
            return true;
        });
}

void SettingsHelpView::present()
{
    playConfirmSfx();
    PushedActivity::push(new SettingsHelpView());
}

} // namespace sf::ui
