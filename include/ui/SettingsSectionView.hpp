#pragma once

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>

namespace sf::ui {

/** Scrollable settings sub-page (uses settings.xml layout). */
class SettingsSectionView : public brls::Box {
public:
    using BuildRowsFn = std::function<void(brls::Box* listBox)>;

    SettingsSectionView(std::string title, std::string subtitle, BuildRowsFn buildRows,
                        std::function<void()> onAppear = nullptr);
    void willAppear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    static void present(std::string title, std::string subtitle, BuildRowsFn buildRows,
                        std::function<void()> onAppear = nullptr);

private:
    void pageScroll(int direction);
    void rebuildContent();
    void refreshTheme();

    std::string title_;
    std::string subtitle_;
    BuildRowsFn buildRows_;
    std::function<void()> onAppear_;

    uint32_t themeGenApplied_ = 0;

    BRLS_BIND(brls::ScrollingFrame, scroller_, "settings/scroller");
    BRLS_BIND(brls::Box, listBox_, "settings/list");
};

void buildAppLauncherSettingsRows(brls::Box* listBox);
void buildEmulatorSystemsSettingsRows(brls::Box* listBox);
void buildAppearanceControlSettingsRows(brls::Box* listBox);
void buildCloudSaveSettingsRows(brls::Box* listBox);
void buildScraperSettingsRows(brls::Box* listBox);
void buildHelpAboutSettingsRows(brls::Box* listBox);

void refreshCloudSaveSettingsSection();

} // namespace sf::ui
