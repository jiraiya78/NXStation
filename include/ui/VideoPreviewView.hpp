#pragma once

#include <borealis.hpp>
#include <vector>

namespace sf::ui {

/** Displays the shared VideoPlayer frame using a NanoVG texture (main thread only). */
class VideoPreviewView : public brls::View {
public:
    VideoPreviewView();
    ~VideoPreviewView() override;

    void frame(brls::FrameContext* ctx) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    /**
     * Uploads the newest decoded frame, or drops the texture once playback stops.
     * Called from frame(), and separately by parents that hide this view while there is
     * no video — a hidden view never gets frame(), so it could never start showing one.
     */
    void pump(brls::FrameContext* ctx);

    /** True while a decoded frame is uploaded and being drawn. */
    bool showingVideo() const { return hasFrame_; }

private:
    void releaseTexture(NVGcontext* vg);
    void uploadFrame(NVGcontext* vg, const std::vector<uint8_t>& rgba, int width, int height);

    int nvgImage_ = 0;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    bool hasFrame_ = false;
};

} // namespace sf::ui
