#pragma once

#include <borealis/views/image.hpp>

namespace sf::ui {

/** Image that can scale from its center while remaining clipped to its bounds. */
class ZoomImage : public brls::Image {
public:
    ZoomImage();
    void setZoom(float zoom);
    float zoom() const { return zoom_; }
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    static brls::View* create();

private:
    float zoom_ = 1.f;
};

void setImageZoom(brls::Image* image, float zoom);
void registerZoomImageView();

} // namespace sf::ui
