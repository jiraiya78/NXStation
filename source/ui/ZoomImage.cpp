#include "ui/ZoomImage.hpp"

#include <borealis.hpp>
#include <cmath>
#include <nanovg.h>

namespace sf::ui {

ZoomImage::ZoomImage() = default;

void ZoomImage::setZoom(float zoom)
{
    zoom_ = zoom < 0.01f ? 0.01f : zoom;
    invalidate();
}

void ZoomImage::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                     brls::FrameContext* ctx)
{
    if (std::abs(zoom_ - 1.f) < 0.001f) {
        brls::Image::draw(vg, x, y, width, height, style, ctx);
        return;
    }

    nvgSave(vg);
    nvgIntersectScissor(vg, x, y, width, height);
    nvgTranslate(vg, x + width * 0.5f, y + height * 0.5f);
    nvgScale(vg, zoom_, zoom_);
    nvgTranslate(vg, -(x + width * 0.5f), -(y + height * 0.5f));
    brls::Image::draw(vg, x, y, width, height, style, ctx);
    nvgRestore(vg);
}

brls::View* ZoomImage::create()
{
    return new ZoomImage();
}

void setImageZoom(brls::Image* image, float zoom)
{
    if (auto* zoomed = dynamic_cast<ZoomImage*>(image))
        zoomed->setZoom(zoom);
}

void registerZoomImageView()
{
    brls::Application::registerXMLView("brls:ZoomImage", ZoomImage::create);
    brls::Application::registerXMLView("ZoomImage", ZoomImage::create);
}

} // namespace sf::ui
