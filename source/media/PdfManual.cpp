#include "media/PdfManual.hpp"

#include "util/Logger.hpp"

#include <algorithm>
#include <cmath>

#ifdef SF_HAVE_MUPDF
#include "mupdf/fitz.h"
#endif

namespace sf {

#ifdef SF_HAVE_MUPDF

struct PdfManual::Impl {
    fz_context* ctx = nullptr;
    fz_document* doc = nullptr;
};

PdfManual::PdfManual() : impl_(std::make_unique<Impl>()) {}

PdfManual::~PdfManual()
{
    close();
}

bool PdfManual::open(const std::string& pdfPath)
{
    close();
    if (pdfPath.empty())
        return false;

    auto* impl = impl_.get();
    impl->ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!impl->ctx) {
        SF_LOG_W("PdfManual", "Failed to create MuPDF context");
        return false;
    }

    fz_try(impl->ctx)
    {
        fz_register_document_handlers(impl->ctx);
        impl->doc = fz_open_document(impl->ctx, pdfPath.c_str());
        pageCount_ = fz_count_pages(impl->ctx, impl->doc);
    }
    fz_catch(impl->ctx)
    {
        SF_LOG_W("PdfManual", "Failed to open PDF: %s", pdfPath.c_str());
        fz_drop_context(impl->ctx);
        impl->ctx = nullptr;
        impl->doc = nullptr;
        pageCount_ = 0;
        return false;
    }

    if (pageCount_ <= 0) {
        close();
        return false;
    }

    path_ = pdfPath;
    open_ = true;
    return true;
}

void PdfManual::close()
{
    if (!impl_)
        return;

    auto* impl = impl_.get();
    if (impl->ctx) {
        fz_try(impl->ctx)
        {
            if (impl->doc)
                fz_drop_document(impl->ctx, impl->doc);
        }
        fz_catch(impl->ctx)
        {
        }
        impl->doc = nullptr;
        fz_empty_store(impl->ctx);
        fz_drop_context(impl->ctx);
        impl->ctx = nullptr;
    }

    path_.clear();
    pageCount_ = 0;
    open_ = false;
}

PdfManual::PageImage PdfManual::renderPage(int pageIndex, int targetWidth,
                                           int targetHeight) const
{
    PageImage out;
    if (!open_ || !impl_ || !impl_->ctx || !impl_->doc || pageIndex < 0
        || pageIndex >= pageCount_ || targetWidth <= 0 || targetHeight <= 0)
        return out;

    auto* ctx = impl_->ctx;
    fz_try(ctx)
    {
        fz_page* page = fz_load_page(ctx, impl_->doc, pageIndex);
        fz_rect bounds = fz_bound_page(ctx, page);
        const float pageW = bounds.x1 - bounds.x0;
        const float pageH = bounds.y1 - bounds.y0;
        if (pageW <= 0.f || pageH <= 0.f) {
            fz_drop_page(ctx, page);
            return out;
        }

        const float scale = std::min(static_cast<float>(targetWidth) / pageW,
                                     static_cast<float>(targetHeight) / pageH);
        const int pixW = std::max(1, static_cast<int>(std::lround(pageW * scale)));
        const int pixH = std::max(1, static_cast<int>(std::lround(pageH * scale)));

        fz_matrix transform = fz_scale(scale, scale);
        fz_irect bbox = fz_make_irect(0, 0, pixW, pixH);
        fz_pixmap* pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, pix, 0xFF);
        fz_device* dev = fz_new_draw_device(ctx, transform, pix);
        fz_run_page(ctx, page, dev, fz_identity, nullptr);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        fz_drop_page(ctx, page);

        out.width = pixW;
        out.height = pixH;
        out.rgba.resize(static_cast<size_t>(pixW) * static_cast<size_t>(pixH) * 4);
        const int stride = pix->stride;
        const int n = pix->n;
        const unsigned char* src = pix->samples;
        for (int y = 0; y < pixH; ++y) {
            const unsigned char* row = src + y * stride;
            for (int x = 0; x < pixW; ++x) {
                const unsigned char* px = row + x * n;
                const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(pixW)
                                     + static_cast<size_t>(x))
                                  * 4;
                out.rgba[di + 0] = px[0];
                out.rgba[di + 1] = px[1];
                out.rgba[di + 2] = px[2];
                out.rgba[di + 3] = n >= 4 ? px[3] : 255;
            }
        }
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx)
    {
        SF_LOG_W("PdfManual", "Failed to render page %d", pageIndex);
        out = {};
    }

    return out;
}

#else

struct PdfManual::Impl {};

PdfManual::PdfManual() : impl_(std::make_unique<Impl>()) {}
PdfManual::~PdfManual() = default;

bool PdfManual::open(const std::string&)
{
    return false;
}

void PdfManual::close() {}

PdfManual::PageImage PdfManual::renderPage(int, int, int) const
{
    return {};
}

#endif

} // namespace sf
