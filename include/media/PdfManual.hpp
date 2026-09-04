#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sf {

/** On-demand PDF page rasterization for the manual viewer (MuPDF). */
class PdfManual {
public:
    struct PageImage {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;
    };

    PdfManual();
    ~PdfManual();

    PdfManual(const PdfManual&) = delete;
    PdfManual& operator=(const PdfManual&) = delete;

    bool open(const std::string& pdfPath);
    void close();

    bool isOpen() const { return open_; }
    int pageCount() const { return pageCount_; }
    const std::string& path() const { return path_; }

    /** Render a page to RGBA at the given pixel size (aspect-fit inside box). */
    PageImage renderPage(int pageIndex, int targetWidth, int targetHeight) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string path_;
    int pageCount_ = 0;
    bool open_ = false;
};

} // namespace sf
