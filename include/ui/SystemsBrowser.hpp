#pragma once

#include <borealis.hpp>

namespace sf::ui {

/** Root systems view — hosts carousel or list layout based on settings/theme. */
class SystemsBrowser : public brls::Box {
public:
    SystemsBrowser();
    ~SystemsBrowser() override = default;

    static brls::View* create();

    void willAppear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    static void requestRefreshAfterSettings();

private:
    void rebuildChild();
    void syncChildFocus();

    brls::View* child_ = nullptr;
    int activeStyle_ = -1;
    uint32_t themeGenApplied_ = 0;
};

} // namespace sf::ui
