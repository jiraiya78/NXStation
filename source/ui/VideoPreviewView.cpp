#include "ui/VideoPreviewView.hpp"

#include "app/AppState.hpp"
#include "media/VideoPlayer.hpp"
#include "util/Logger.hpp"

#include <utility>

namespace sf::ui {

VideoPreviewView::VideoPreviewView()
{
    // Fills the art frame it is added to, so the preview replaces the box art in place.
    this->setAlignSelf(brls::AlignSelf::STRETCH);
    this->setGrow(1.0f);
}

VideoPreviewView::~VideoPreviewView()
{
    NVGcontext* vg = brls::Application::getNVGContext();
    if (vg)
        releaseTexture(vg);
}

void VideoPreviewView::releaseTexture(NVGcontext* vg)
{
    if (nvgImage_ != 0) {
        nvgDeleteImage(vg, nvgImage_);
        nvgImage_ = 0;
    }
    imageWidth_ = 0;
    imageHeight_ = 0;
    hasFrame_ = false;
}

void VideoPreviewView::uploadFrame(NVGcontext* vg, const std::vector<uint8_t>& rgba, int width,
                                   int height)
{
    constexpr int kMaxW = VideoPlayer::kMaxPreviewWidth;
    constexpr int kMaxH = VideoPlayer::kMaxPreviewHeight;

    if (width <= 0 || height <= 0 || rgba.empty() || width > kMaxW || height > kMaxH) {
        if (width > kMaxW || height > kMaxH)
            SF_LOG_W("Video", "Frame too large for GPU upload (%dx%d)", width, height);
        releaseTexture(vg);
        return;
    }

    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (rgba.size() != expected) {
        SF_LOG_W("Video", "Frame buffer size mismatch (%zu vs %zu)", rgba.size(), expected);
        releaseTexture(vg);
        return;
    }

    if (nvgImage_ == 0 || width != imageWidth_ || height != imageHeight_) {
        releaseTexture(vg);
        nvgImage_ = nvgCreateImageRGBA(vg, width, height, 0, rgba.data());
        imageWidth_ = width;
        imageHeight_ = height;
        if (nvgImage_ != 0)
            SF_LOG_I("Video", "Frame uploaded to GPU (%dx%d)", width, height);
        else
            SF_LOG_E("Video", "nvgCreateImageRGBA failed (%dx%d)", width, height);
    } else {
        nvgUpdateImage(vg, nvgImage_, rgba.data());
    }

    hasFrame_ = nvgImage_ != 0;
}

void VideoPreviewView::pump(brls::FrameContext* ctx)
{
    if (!ctx || !ctx->vg)
        return;

    auto& player = AppState::instance().video();

    VideoFrame vf;
    if (player.enabled() && player.tryConsumeFrame(vf)) {
        uploadFrame(ctx->vg, vf.rgba, vf.width, vf.height);
        player.recycleFrame(std::move(vf));
    } else if (hasFrame_ && !player.playing())
        releaseTexture(ctx->vg); // playback stopped: fall back to the box art
}

void VideoPreviewView::frame(brls::FrameContext* ctx)
{
    pump(ctx);
    brls::View::frame(ctx);
}

void VideoPreviewView::draw(NVGcontext* vg, float x, float y, float width, float height,
                            brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;

    if (!hasFrame_ || nvgImage_ == 0 || imageWidth_ <= 0 || imageHeight_ <= 0 || width <= 0 ||
        height <= 0)
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

    NVGpaint paint = nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0, nvgImage_, 1.0f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, drawX, drawY, drawW, drawH, 8.f);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

} // namespace sf::ui
