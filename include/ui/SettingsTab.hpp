#pragma once

#include <borealis.hpp>

#include <memory>

namespace sf::ui {

class SettingsTab : public brls::Box {
public:
    SettingsTab();
    ~SettingsTab() override = default;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    static brls::View* create();
    static void open();

private:
    void rebuild();
    void applyThemeStyles();
    void pageScroll(int direction);
    void onFocusRowChanged(brls::View* focused);

    brls::View* lastFocusedRow_ = nullptr;
    uint32_t themeGenApplied_ = 0;
    bool built_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);

    BRLS_BIND(brls::ScrollingFrame, scroller, "settings/scroller");
    BRLS_BIND(brls::Box, listBox, "settings/list");
};

} // namespace sf::ui
