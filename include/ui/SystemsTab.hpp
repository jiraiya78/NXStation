#pragma once

#include "ui/HaloLabel.hpp"
#include "util/IdleTimer.hpp"

#include <borealis.hpp>
#include <atomic>
#include <vector>
namespace sf::ui {

class SystemsTab : public brls::Box {
public:
    SystemsTab();
    ~SystemsTab() override = default;

    static brls::View* create();

    void willAppear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    /** Called when settings closes so hide-empty-systems applies immediately. */
    static void requestRefreshAfterSettings();

private:
    void rebuildSystemIds();
    void showSystem(size_t index);
    void stepSystem(int delta);
    void beginSystemTransition(size_t nextIndex, int direction);
    void beginSystemFade(size_t nextIndex);
    void beginSystemSlide(size_t nextIndex, int direction);
    void beginSystemCrossfade(size_t nextIndex);
    void beginSystemZoom(size_t nextIndex);
    void resetBackgroundTransform();
    void setSystemBackground(brls::Image* image, const std::string& systemId);
    void updateSystemLabels(size_t index);
    size_t initialSystemIndex() const;
    void tickStickNavigation(float delta);

    std::vector<std::string> systemIds_;
    size_t index_ = 0;
    uint32_t themeGenApplied_ = 0;
    IdleTimer idleTimer_;
    brls::Time lastFrameTime_ = 0;
    bool stickHeld_ = false;
    bool stickRepeating_ = false;
    int stickDir_ = 0;
    float stickTimer_ = 0.f;
    bool transitioning_ = false;
    brls::Animatable fadeAnim_{0.f};
    brls::Animatable slideAnim_{0.f};

    BRLS_BIND(brls::Image, bgImage, "carousel/bg");
    BRLS_BIND(brls::Image, bgImageNext, "carousel/bg_next");
    BRLS_BIND(brls::Label, titleLabel, "carousel/title");
    BRLS_BIND(brls::Label, countLabel, "carousel/count");
    BRLS_BIND(brls::Label, hintsLabel, "carousel/hints");
    BRLS_BIND(brls::Rectangle, shadeOverlay, "carousel/shade");
    OverlayGradient* textShade_ = nullptr;

    void applyThemeColors();
    void tickIdleScreensaver();
};

} // namespace sf::ui
