#include "ui/SettingsHelpers.hpp"

#include "app/Config.hpp"
#include "ui/ThemeManager.hpp"

#include <borealis/views/header.hpp>
#include <borealis/views/rectangle.hpp>

#include <cmath>
#include <functional>

namespace sf::ui {

namespace {

void styleHeaderTree(brls::View* view)
{
    if (!view)
        return;

    auto& theme = ThemeManager::instance();
    int labelIndex = 0;

    std::function<void(brls::View*)> walk = [&](brls::View* node) {
        if (auto* label = dynamic_cast<brls::Label*>(node)) {
            label->setTextColor(labelIndex == 0 ? theme.color("brls/text")
                                                : theme.color("brls/header/subtitle"));
            ++labelIndex;
        } else if (auto* rect = dynamic_cast<brls::Rectangle*>(node)) {
            rect->setColor(theme.color("brls/header/rectangle"));
        }

        if (auto* box = dynamic_cast<brls::Box*>(node)) {
            box->setLineColor(theme.color("brls/header/border"));
            for (brls::View* child : box->getChildren())
                walk(child);
        }
    };

    walk(view);
}

} // namespace

void styleSettingsCell(brls::DetailCell* cell)
{
    cell->setTextColor(ThemeManager::instance().color("nxstation/title_text"));
    cell->setDetailTextColor(ThemeManager::instance().color("nxstation/detail_text"));
    cell->setFocusable(true);
}

brls::DetailCell* makeSettingsRow(const std::string& title, const std::string& detail)
{
    auto* row = new brls::DetailCell();
    row->setText(title);
    if (!detail.empty())
        row->setDetailText(detail);
    styleSettingsCell(row);
    return row;
}

void applyThemeToSettingsPanel(brls::View* panel, brls::Box* listBox)
{
    auto& theme = ThemeManager::instance();
    if (panel)
        panel->setBackgroundColor(theme.color("brls/background"));

    if (!listBox)
        return;

    for (brls::View* child : listBox->getChildren()) {
        if (auto* cell = dynamic_cast<brls::DetailCell*>(child))
            styleSettingsCell(cell);
        else if (auto* header = dynamic_cast<brls::Header*>(child))
            styleHeaderTree(header);
    }

    if (panel)
        panel->invalidate();
}

void focusFirstSettingsRow(brls::Box* listBox)
{
    if (!listBox)
        return;

    for (brls::View* child : listBox->getChildren()) {
        auto* cell = dynamic_cast<brls::DetailCell*>(child);
        if (!cell)
            continue;
        if (!cell->isFocusable())
            cell->setFocusable(true);
        brls::Application::giveFocus(cell);
        return;
    }
}

static const float kVolumeSteps[] = {0.f, 0.25f, 0.5f, 0.75f, 1.0f};
static constexpr size_t kVolumeStepCount = sizeof(kVolumeSteps) / sizeof(kVolumeSteps[0]);

std::string settingsVolumeLabel(float volume)
{
    if (volume <= 0.001f)
        return "Off";
    if (volume <= 0.26f)
        return "25%";
    if (volume <= 0.51f)
        return "50%";
    if (volume <= 0.76f)
        return "75%";
    return "100%";
}

float nextSettingsVolumeStep(float current)
{
    for (size_t i = 0; i < kVolumeStepCount; ++i) {
        if (std::abs(current - kVolumeSteps[i]) < 0.02f)
            return kVolumeSteps[(i + 1) % kVolumeStepCount];
    }
    return kVolumeSteps[1];
}

std::string settingsGameArtLabel(GameArtMode mode)
{
    return mode == GameArtMode::Thumbnail ? "Thumbnail" : "Box Art";
}

static const int kScreensaverPresets[] = {0, 60, 120, 180, 300, 600};

std::string settingsScreensaverLabel(int seconds)
{
    if (seconds <= 0)
        return "Off";
    if (seconds % 60 == 0)
        return std::to_string(seconds / 60) + " min";
    return std::to_string(seconds) + " sec";
}

int nextSettingsScreensaverPreset(int current)
{
    constexpr size_t count = sizeof(kScreensaverPresets) / sizeof(kScreensaverPresets[0]);
    for (size_t i = 0; i < count; ++i) {
        if (kScreensaverPresets[i] == current)
            return kScreensaverPresets[(i + 1) % count];
    }
    return 120;
}

std::string settingsVideoPreviewDelayLabel(int seconds)
{
    return std::to_string(seconds) + " sec";
}

int nextSettingsVideoPreviewDelay(int current)
{
    static const int kOptions[] = {1, 2, 3, 4, 5, 6};
    constexpr size_t count = sizeof(kOptions) / sizeof(kOptions[0]);
    for (size_t i = 0; i < count; ++i) {
        if (current == kOptions[i])
            return kOptions[(i + 1) % count];
    }
    return 1;
}

std::string settingsCloudLastBackupLabel()
{
    const std::string iso = Config::instance().cloudLastSyncIso();
    return iso.empty() ? "Never" : iso;
}

} // namespace sf::ui
