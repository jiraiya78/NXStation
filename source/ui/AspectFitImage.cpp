#include "ui/AspectFitImage.hpp"

#include "util/FileSystem.hpp"

#ifdef USE_LIBROMFS
#include <romfs/romfs.hpp>
#endif

namespace sf::ui {

AspectFitImage::AspectFitImage()
{
    setClipsToBounds(true);
}

AspectFitImage::~AspectFitImage()
{
    if (NVGcontext* vg = brls::Application::getNVGContext())
        releaseTexture(vg);
}

void AspectFitImage::setCornerRadius(float radius)
{
    cornerRadius_ = radius;
}

void AspectFitImage::clearImage()
{
    if (NVGcontext* vg = brls::Application::getNVGContext())
        releaseTexture(vg);
    path_.clear();
    fromRes_ = false;
}

void AspectFitImage::setImageFromRes(const std::string& resPath)
{
#ifdef USE_LIBROMFS
    if (resPath.empty()) {
        clearImage();
        return;
    }
    if (fromRes_ && path_ == resPath && texture_ != 0)
        return;
    auto image = romfs::get(resPath);
    if (!image.valid()) {
        clearImage();
        return;
    }
    path_ = resPath;
    fromRes_ = true;
    if (NVGcontext* vg = brls::Application::getNVGContext())
        setImageFromMemory(reinterpret_cast<const unsigned char*>(image.data()),
                           static_cast<int>(image.size()));
#else
    setImageFromFile(std::string(BRLS_RESOURCES) + resPath);
#endif
}

void AspectFitImage::setImageFromFile(const std::string& path)
{
    if (path.empty() || !FileSystem::exists(path)) {
        clearImage();
        return;
    }
    if (!fromRes_ && path == path_ && texture_ != 0)
        return;
    fromRes_ = false;
    path_ = path;
    if (NVGcontext* vg = brls::Application::getNVGContext())
        ensureTexture(vg);
}

void AspectFitImage::setImageFromMemory(const unsigned char* data, int size)
{
    NVGcontext* vg = brls::Application::getNVGContext();
    if (!vg || !data || size <= 0) {
        clearImage();
        return;
    }
    releaseTexture(vg);
    texture_ = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(data), size);
    if (texture_ == 0)
        return;
    nvgImageSize(vg, texture_, &imageWidth_, &imageHeight_);
}

void AspectFitImage::releaseTexture(NVGcontext* vg)
{
    if (texture_ != 0) {
        nvgDeleteImage(vg, texture_);
        texture_ = 0;
    }
    imageWidth_ = 0;
    imageHeight_ = 0;
}

void AspectFitImage::ensureTexture(NVGcontext* vg)
{
    if (!vg || texture_ != 0 || fromRes_ || path_.empty())
        return;
    texture_ = nvgCreateImage(vg, path_.c_str(), 0);
    if (texture_ == 0)
        return;
    nvgImageSize(vg, texture_, &imageWidth_, &imageHeight_);
}

void AspectFitImage::draw(NVGcontext* vg, float x, float y, float width, float height,
                          brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;

    if (width <= 0.f || height <= 0.f)
        return;

    ensureTexture(vg);
    if (texture_ == 0 || imageWidth_ <= 0 || imageHeight_ <= 0)
        return;

    const float viewAspect = width / height;
    const float imageAspect = static_cast<float>(imageWidth_) / static_cast<float>(imageHeight_);

    float drawW = width;
    float drawH = height;
    float drawX = x;
    float drawY = y;

    if (viewAspect >= imageAspect) {
        drawH = height;
        drawW = drawH * imageAspect;
        drawX = x + (width - drawW) * 0.5f;
    } else {
        drawW = width;
        drawH = drawW / imageAspect;
        drawY = y + (height - drawH) * 0.5f;
    }

    NVGpaint paint = nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0, texture_, 1.f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, drawX, drawY, drawW, drawH, cornerRadius_);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

} // namespace sf::ui
