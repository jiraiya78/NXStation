#pragma once

#include <borealis.hpp>
#include <string>

namespace sf::ui {

/** Draws a file image with correct aspect-fit (avoids brls::Image edge stretch artifacts). */
class AspectFitImage : public brls::View {
public:
    AspectFitImage();
    ~AspectFitImage() override;

    void setImageFromFile(const std::string& path);
    void setImageFromRes(const std::string& resPath);
    void clearImage();
    void setCornerRadius(float radius);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

private:
    void releaseTexture(NVGcontext* vg);
    void ensureTexture(NVGcontext* vg);
    void setImageFromMemory(const unsigned char* data, int size);

    std::string path_;
    bool fromRes_ = false;
    int texture_ = 0;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    float cornerRadius_ = 0.f;
};

} // namespace sf::ui
