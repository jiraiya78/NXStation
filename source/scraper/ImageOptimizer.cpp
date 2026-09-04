#include "scraper/ImageOptimizer.hpp"

#include "util/Logger.hpp"

#include <borealis/extern/nanovg/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace sf {

namespace {

constexpr int kBoxMaxWidth = 512;
constexpr int kThumbMaxWidth = 320;
constexpr int kBoxJpegQuality = 80;
constexpr int kThumbJpegQuality = 78;

void resizeBilinear(const uint8_t* src, int srcW, int srcH, int channels, uint8_t* dst, int dstW,
                    int dstH)
{
    const float xScale = static_cast<float>(srcW) / static_cast<float>(dstW);
    const float yScale = static_cast<float>(srcH) / static_cast<float>(dstH);

    for (int y = 0; y < dstH; ++y) {
        const float srcY = (static_cast<float>(y) + 0.5f) * yScale - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(srcY)), 0, srcH - 1);
        const int y1 = std::min(y0 + 1, srcH - 1);
        const float fy = srcY - static_cast<float>(y0);

        for (int x = 0; x < dstW; ++x) {
            const float srcX = (static_cast<float>(x) + 0.5f) * xScale - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(srcX)), 0, srcW - 1);
            const int x1 = std::min(x0 + 1, srcW - 1);
            const float fx = srcX - static_cast<float>(x0);

            for (int c = 0; c < channels; ++c) {
                const float p00 = src[(y0 * srcW + x0) * channels + c];
                const float p10 = src[(y0 * srcW + x1) * channels + c];
                const float p01 = src[(y1 * srcW + x0) * channels + c];
                const float p11 = src[(y1 * srcW + x1) * channels + c];
                const float top = p00 + (p10 - p00) * fx;
                const float bottom = p01 + (p11 - p01) * fx;
                const float value = top + (bottom - top) * fy;
                dst[(y * dstW + x) * channels + c] =
                    static_cast<uint8_t>(std::clamp(value, 0.f, 255.f));
            }
        }
    }
}

std::vector<uint8_t> toRgb(const uint8_t* src, int w, int h, int channels)
{
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
    for (int i = 0; i < w * h; ++i) {
        rgb[static_cast<size_t>(i) * 3 + 0] = src[static_cast<size_t>(i) * channels + 0];
        rgb[static_cast<size_t>(i) * 3 + 1] =
            channels > 1 ? src[static_cast<size_t>(i) * channels + 1] : src[static_cast<size_t>(i) * channels + 0];
        rgb[static_cast<size_t>(i) * 3 + 2] =
            channels > 2 ? src[static_cast<size_t>(i) * channels + 2] : src[static_cast<size_t>(i) * channels + 0];
    }
    return rgb;
}

} // namespace

bool optimizeScrapedImage(const std::string& path, bool thumbnail)
{
    int w = 0;
    int h = 0;
    int channels = 0;
    uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels)
            stbi_image_free(pixels);
        SF_LOG_W("Scraper", "Optimize skipped (decode failed): %s", path.c_str());
        return false;
    }

    const int maxWidth = thumbnail ? kThumbMaxWidth : kBoxMaxWidth;
    int outW = w;
    int outH = h;
    if (w > maxWidth) {
        outW = maxWidth;
        outH = std::max(1, static_cast<int>(std::lround(static_cast<double>(h) * maxWidth / w)));
    }

    std::vector<uint8_t> resized;
    const uint8_t* source = pixels;
    if (outW != w || outH != h) {
        resized.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * channels);
        resizeBilinear(pixels, w, h, channels, resized.data(), outW, outH);
        source = resized.data();
    }

    const std::vector<uint8_t> rgb = toRgb(source, outW, outH, channels);
    const int quality = thumbnail ? kThumbJpegQuality : kBoxJpegQuality;
    const int ok = stbi_write_jpg(path.c_str(), outW, outH, 3, rgb.data(), quality);

    stbi_image_free(pixels);

    if (!ok) {
        SF_LOG_W("Scraper", "Optimize write failed: %s", path.c_str());
        return false;
    }

    SF_LOG_I("Scraper", "Optimized %s (%dx%d q=%d)", path.c_str(), outW, outH, quality);
    return true;
}

} // namespace sf
