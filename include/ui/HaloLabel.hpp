#pragma once

#include <borealis.hpp>
#include <nanovg.h>

namespace sf::ui {

/** Label with a contrast halo/outline so it stays readable on busy artwork. */
class HaloLabel : public brls::Label {
public:
    HaloLabel();
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    static brls::View* create();
};

/** Bottom-to-top fade used under carousel titles. */
class OverlayGradient : public brls::View {
public:
    OverlayGradient();
    void setColor(NVGcolor color);
    void setStrength(float strength);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    static brls::View* create();

private:
    NVGcolor color_{0.f, 0.f, 0.f, 1.f};
    float strength_ = 0.72f;
};

void registerHaloLabelView();

} // namespace sf::ui
