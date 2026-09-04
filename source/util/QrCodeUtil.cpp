#include "util/QrCodeUtil.hpp"

#include "stb_image_write.h"

#include "qrcodegen.hpp"

#include <vector>

namespace sf {

namespace {

void pngWriteCallback(void* context, void* data, int size)
{
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

} // namespace

std::vector<uint8_t> encodeQrPng(const std::string& text, int moduleScale, int quietBorder)
{
    std::vector<uint8_t> png;
    if (text.empty() || moduleScale < 1 || quietBorder < 0)
        return png;

    try {
        const qrcodegen::QrCode qr =
            qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int modules = qr.getSize();
        const int dim = (modules + quietBorder * 2) * moduleScale;

        std::vector<uint8_t> rgba(static_cast<size_t>(dim) * static_cast<size_t>(dim) * 4, 255);
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                const int mx = x / moduleScale - quietBorder;
                const int my = y / moduleScale - quietBorder;
                bool dark = false;
                if (mx >= 0 && mx < modules && my >= 0 && my < modules)
                    dark = qr.getModule(mx, my);
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(dim) +
                                  static_cast<size_t>(x)) *
                                 4;
                const uint8_t v = dark ? 0 : 255;
                rgba[i] = v;
                rgba[i + 1] = v;
                rgba[i + 2] = v;
                rgba[i + 3] = 255;
            }
        }

        stbi_write_png_to_func(pngWriteCallback, &png, dim, dim, 4, rgba.data(), dim * 4);
    } catch (...) {
        png.clear();
    }

    return png;
}

} // namespace sf
