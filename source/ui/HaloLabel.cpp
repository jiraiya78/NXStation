#include "ui/HaloLabel.hpp"

#include <algorithm>
#include <cmath>

namespace sf::ui {

HaloLabel::HaloLabel() = default;

void HaloLabel::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                     brls::FrameContext* ctx)
{
    if (width == 0.f)
        return;

    const NVGcolor fg = textColor;
    const float lum = 0.2126f * fg.r + 0.7152f * fg.g + 0.0722f * fg.b;
    const bool lightText = lum > 0.55f;
    const NVGcolor halo =
        lightText ? nvgRGBAf(0.f, 0.f, 0.f, 0.92f) : nvgRGBAf(1.f, 1.f, 1.f, 0.92f);
    const float outline = std::max(1.6f, getFontSize() * 0.045f);

    textColor = halo;
    nvgSave(vg);
    nvgFontBlur(vg, std::max(5.f, getFontSize() * 0.18f));
    brls::Label::draw(vg, x, y, width, height, style, ctx);
    nvgRestore(vg);

    static const float kOx[] = { -1.f, 1.f, 0.f, 0.f, -1.f, 1.f, -1.f, 1.f };
    static const float kOy[] = { 0.f, 0.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f };
    for (int i = 0; i < 8; ++i)
        brls::Label::draw(vg, x + kOx[i] * outline, y + kOy[i] * outline, width, height, style, ctx);

    textColor = fg;
    nvgFontBlur(vg, 0.f);
    brls::Label::draw(vg, x, y, width, height, style, ctx);
}

brls::View* HaloLabel::create()
{
    return new HaloLabel();
}

OverlayGradient::OverlayGradient()
{
    setFocusable(false);
}

void OverlayGradient::setColor(NVGcolor color)
{
    color_ = color;
    invalidate();
}

void OverlayGradient::setStrength(float strength)
{
    strength_ = std::clamp(strength, 0.f, 1.f);
    invalidate();
}

void OverlayGradient::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style,
                           brls::FrameContext* ctx)
{
    NVGcolor top = color_;
    top.a = 0.f;
    NVGcolor bottom = color_;
    bottom.a = strength_;
    NVGpaint paint = nvgLinearGradient(vg, x, y, x, y + height, a(top), a(bottom));
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    (void)ctx;
}

brls::View* OverlayGradient::create()
{
    return new OverlayGradient();
}

void registerHaloLabelView()
{
    brls::Application::registerXMLView("brls:HaloLabel", HaloLabel::create);
    brls::Application::registerXMLView("HaloLabel", HaloLabel::create);
    brls::Application::registerXMLView("brls:OverlayGradient", OverlayGradient::create);
    brls::Application::registerXMLView("OverlayGradient", OverlayGradient::create);
}

} // namespace sf::ui
