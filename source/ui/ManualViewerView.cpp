#include "ui/ManualViewerView.hpp"



#include "app/Config.hpp"
#include "media/PdfManual.hpp"

#include "ui/FocusedMenuDialog.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/Logger.hpp"



#include <borealis/core/touch/pan_gesture.hpp>



#include <algorithm>

#include <cmath>



#ifdef __SWITCH__

#include <switch.h>

#endif



namespace sf::ui {



namespace {



constexpr float kFrameW = 1280.f;

constexpr float kFrameH = 720.f;

constexpr float kMinZoom = 1.f;

constexpr float kMaxZoom = 4.f;

constexpr float kSwipeThreshold = 72.f;

constexpr float kRenderScale = 2.f;



float touchDistance(const brls::Point& a, const brls::Point& b)

{

    const float dx = a.x - b.x;

    const float dy = a.y - b.y;

    return std::sqrt(dx * dx + dy * dy);

}



bool pointInside(const brls::Rect& rect, const brls::Point& p)

{

    return p.x >= rect.getMinX() && p.x <= rect.getMaxX() && p.y >= rect.getMinY()

           && p.y <= rect.getMaxY();

}



struct LayoutRect {

    float x = 0.f;

    float y = 0.f;

    float w = 0.f;

    float h = 0.f;

};



LayoutRect aspectFitRect(float slotX, float slotY, float slotW, float slotH, int imgW, int imgH)
{
    LayoutRect r;
    if (slotW <= 0.f || slotH <= 0.f || imgW <= 0 || imgH <= 0)
        return r;

    const float viewAspect = slotW / slotH;
    const float imageAspect = static_cast<float>(imgW) / static_cast<float>(imgH);
    if (viewAspect >= imageAspect) {
        r.h = slotH;
        r.w = r.h * imageAspect;
        r.x = slotX + (slotW - r.w) * 0.5f;
        r.y = slotY;
    } else {
        r.w = slotW;
        r.h = r.w / imageAspect;
        r.x = slotX;
        r.y = slotY + (slotH - r.h) * 0.5f;
    }
    return r;
}

/** Place spread pages edge-to-edge (book style) with no center gutter. */
void layoutSpreadRects(float width, float height, int leftImgW, int leftImgH, int rightImgW,
                       int rightImgH, LayoutRect& left, LayoutRect& right)
{
    left = {};
    right = {};
    if (width <= 0.f || height <= 0.f || leftImgW <= 0 || leftImgH <= 0)
        return;

    const bool hasRight = rightImgW > 0 && rightImgH > 0;
    if (!hasRight) {
        left = aspectFitRect(0.f, 0.f, width, height, leftImgW, leftImgH);
        return;
    }

    const float leftAspect = static_cast<float>(leftImgW) / static_cast<float>(leftImgH);
    const float rightAspect = static_cast<float>(rightImgW) / static_cast<float>(rightImgH);
    const float combinedAspect = leftAspect + rightAspect;
    const float viewAspect = width / height;

    float totalW = 0.f;
    float totalH = 0.f;
    if (viewAspect >= combinedAspect) {
        totalH = height;
        totalW = totalH * combinedAspect;
    } else {
        totalW = width;
        totalH = totalW / combinedAspect;
    }

    const float leftDrawW = totalW * (leftAspect / combinedAspect);
    const float rightDrawW = totalW - leftDrawW;
    const float ox = (width - totalW) * 0.5f;
    const float oy = (height - totalH) * 0.5f;

    left = {ox, oy, leftDrawW, totalH};
    right = {ox + leftDrawW, oy, rightDrawW, totalH};
}



} // namespace



class ManualViewerView::ManualPageCanvas : public brls::Box {

public:

    ManualPageCanvas()

    {

        setGrow(1.f);

        setClipsToBounds(true);

        setFocusable(true);

        setHideHighlight(true);



        addGestureRecognizer(new brls::PanGestureRecognizer(

            [this](brls::PanGestureStatus status, brls::Sound*) {

                if (zoom_ > 1.02f)

                    return;

                if (status.state == brls::GestureState::END) {

                    const float dx = status.position.x - status.startPosition.x;

                    if (dx <= -kSwipeThreshold)

                        onSwipeNext_();

                    else if (dx >= kSwipeThreshold)

                        onSwipePrev_();

                }

            },

            brls::PanAxis::HORIZONTAL));

    }



    ~ManualPageCanvas() override

    {

        if (NVGcontext* vg = brls::Application::getNVGContext())

            releaseTextures(vg);

    }



    void releaseGpuResources(NVGcontext* vg)
    {
        releaseTextures(vg);
    }

    void setDocument(PdfManual* pdf)

    {

        pdf_ = pdf;

    }



    void setSpread(int leftPage, int rightPage)

    {

        leftPage_ = leftPage;

        rightPage_ = rightPage;

        resetViewTransform();

    }



    bool beginSpreadTransition(int newLeft, int newRight, int direction,
                               std::function<void()> onComplete)

    {

        if (transitionActive_)

            return false;



        NVGcontext* vg = brls::Application::getNVGContext();

        if (!vg || !pdf_ || !pdf_->isOpen())

            return false;



        const brls::Rect frame = getFrame();

        if (frame.getWidth() < 8.f || frame.getHeight() < 8.f)

            return false;



        // Always flatten zoom/pan so the page turn is a clean horizontal slide.

        resetViewTransform();



        outgoingLeftPage_ = leftPage_;

        outgoingRightPage_ = rightPage_;

        outgoingLeftTex_ = leftTex_;

        outgoingRightTex_ = rightTex_;

        outgoingLeftW_ = leftW_;

        outgoingLeftH_ = leftH_;

        outgoingRightW_ = rightW_;

        outgoingRightH_ = rightH_;



        leftTex_ = 0;

        rightTex_ = 0;

        leftPageCached_ = -1;

        rightPageCached_ = -1;

        leftSlotTargetW_ = leftSlotTargetH_ = 0;

        rightSlotTargetW_ = rightSlotTargetH_ = 0;

        leftW_ = leftH_ = rightW_ = rightH_ = 0;



        leftPage_ = newLeft;

        rightPage_ = newRight;

        ensureTextures(vg);



        transitionDir_ = direction >= 0 ? 1 : -1;

        transitionDone_ = std::move(onComplete);

        transitionActive_ = true;



        constexpr int kDurationMs = 280;

        transitionAnim_.stop();

        transitionAnim_.reset(0.f);

        transitionAnim_.addStep(1.f, kDurationMs, brls::EasingFunction::cubicOut);

        transitionAnim_.setTickCallback([this]() { this->invalidate(); });

        transitionAnim_.setEndCallback([this](bool finished) {

            if (!finished) {

                transitionActive_ = false;

                return;

            }

            finishSpreadTransition();

        });

        transitionAnim_.start();

        invalidate();

        return true;

    }



    void setOnSwipe(std::function<void()> onPrev, std::function<void()> onNext)

    {

        onSwipePrev_ = std::move(onPrev);

        onSwipeNext_ = std::move(onNext);

    }



    void adjustZoom(float delta, float focalX, float focalY, float viewX, float viewY, float viewW,

                    float viewH)

    {

        const float oldZoom = zoom_;

        zoom_ = std::clamp(zoom_ + delta, kMinZoom, kMaxZoom);

        if (std::abs(zoom_ - oldZoom) < 0.0001f)

            return;



        const float cx = viewX + viewW * 0.5f;

        const float cy = viewY + viewH * 0.5f;

        const float fx = focalX - cx;

        const float fy = focalY - cy;

        const float ratio = zoom_ / oldZoom;

        panX_ = fx - (fx - panX_) * ratio;

        panY_ = fy - (fy - panY_) * ratio;



        if (zoom_ <= 1.01f) {

            panX_ = 0.f;

            panY_ = 0.f;

        }

    }



    void frame(brls::FrameContext* ctx) override

    {

        brls::Box::frame(ctx);

        handleControllerZoomPan();

        handleTouchZoomPan();

    }



    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,

              brls::FrameContext* ctx) override

    {

        (void)style;

        (void)ctx;



        auto& theme = ThemeManager::instance();

        nvgBeginPath(vg);

        nvgRect(vg, x, y, width, height);

        nvgFillColor(vg, theme.color("brls/background"));

        nvgFill(vg);



        if (!pdf_ || !pdf_->isOpen() || width <= 0.f || height <= 0.f)

            return;



        if (transitionActive_) {

            const float t = std::clamp(transitionAnim_.getValue(), 0.f, 1.f);

            const float slide = width * t;



            nvgSave(vg);

            nvgScissor(vg, x, y, width, height);



            // Outgoing leaves left for next (+1), right for prev (-1).

            drawSpreadPagesAbs(vg, x - transitionDir_ * slide, y, width, height,

                               outgoingLeftTex_, outgoingRightTex_, outgoingLeftW_, outgoingLeftH_,

                               outgoingRightW_, outgoingRightH_, outgoingRightPage_ >= 0);



            // Incoming enters from the opposite side.

            drawSpreadPagesAbs(vg, x + transitionDir_ * (width - slide), y, width, height, leftTex_,

                               rightTex_, leftW_, leftH_, rightW_, rightH_, rightPage_ >= 0);



            nvgRestore(vg);

            return;

        }



        ensureTextures(vg);



        const bool spread = rightPage_ >= 0;
        const float contentW = width;
        const float contentH = height;

        LayoutRect leftLayout{};
        LayoutRect rightLayout{};
        if (leftPage_ >= 0 && leftTex_ != 0) {
            if (spread)
                layoutSpreadRects(contentW, contentH, leftW_, leftH_, rightW_, rightH_, leftLayout,
                                  rightLayout);
            else
                leftLayout = aspectFitRect(0.f, 0.f, contentW, contentH, leftW_, leftH_);
        }



        nvgSave(vg);

        nvgScissor(vg, x, y, width, height);

        nvgTranslate(vg, x + width * 0.5f + panX_, y + height * 0.5f + panY_);

        nvgScale(vg, zoom_, zoom_);

        nvgTranslate(vg, -contentW * 0.5f, -contentH * 0.5f);



        if (leftTex_ != 0 && leftLayout.w > 0.f)

            drawPageTexture(vg, leftLayout, leftTex_, 1.f);

        if (spread && rightTex_ != 0 && rightLayout.w > 0.f)

            drawPageTexture(vg, rightLayout, rightTex_, 1.f);



        nvgRestore(vg);

    }



private:

    void resetViewTransform()

    {

        zoom_ = 1.f;

        panX_ = 0.f;

        panY_ = 0.f;

        pinchActive_ = false;

        panTouchActive_ = false;

        pinchAnchor_ = {};

    }



    void releaseTextures(NVGcontext* vg)

    {

        if (leftTex_ != 0) {

            nvgDeleteImage(vg, leftTex_);

            leftTex_ = 0;

        }

        if (rightTex_ != 0) {

            nvgDeleteImage(vg, rightTex_);

            rightTex_ = 0;

        }

        leftPageCached_ = -1;

        rightPageCached_ = -1;

        leftSlotTargetW_ = leftSlotTargetH_ = 0;

        rightSlotTargetW_ = rightSlotTargetH_ = 0;

        leftW_ = leftH_ = rightW_ = rightH_ = 0;

    }



    bool uploadPageTexture(NVGcontext* vg, int pageIndex, int& tex, int& w, int& h,

                           int& cachedPage, int& cachedSlotW, int& cachedSlotH, float slotW,

                           float slotH)

    {

        if (pageIndex < 0 || !pdf_ || !pdf_->isOpen()) {

            if (tex != 0) {

                nvgDeleteImage(vg, tex);

                tex = 0;

            }

            cachedPage = -1;

            cachedSlotW = cachedSlotH = 0;

            w = h = 0;

            return false;

        }



        const int targetW = std::max(1, static_cast<int>(slotW * kRenderScale));

        const int targetH = std::max(1, static_cast<int>(slotH * kRenderScale));

        if (pageIndex == cachedPage && tex != 0 && cachedSlotW == targetW

            && cachedSlotH == targetH)

            return w > 0 && h > 0;



        const auto image = pdf_->renderPage(pageIndex, targetW, targetH);

        if (image.width <= 0 || image.height <= 0 || image.rgba.empty()) {

            if (tex != 0) {

                nvgDeleteImage(vg, tex);

                tex = 0;

            }

            cachedPage = -1;

            cachedSlotW = cachedSlotH = 0;

            w = h = 0;

            return false;

        }



        if (tex != 0)

            nvgDeleteImage(vg, tex);

        tex = nvgCreateImageRGBA(vg, image.width, image.height, NVG_IMAGE_NEAREST,

                                 image.rgba.data());

        cachedPage = pageIndex;

        cachedSlotW = targetW;

        cachedSlotH = targetH;

        w = image.width;

        h = image.height;

        if (tex != 0)

            invalidate();

        return tex != 0;

    }



    void ensureTextures(NVGcontext* vg)

    {

        if (!vg || !pdf_)

            return;



        const brls::Rect frame = getFrame();
        const float frameW = frame.getWidth();
        const float frameH = frame.getHeight();
        if (frameW < 8.f || frameH < 8.f)
            return;

        const bool spread = rightPage_ >= 0;

        float leftSlotW = frameW;
        float rightSlotW = frameW;
        if (spread && leftW_ > 0 && rightW_ > 0) {
            const float combined = static_cast<float>(leftW_) + static_cast<float>(rightW_);
            leftSlotW = frameW * (static_cast<float>(leftW_) / combined);
            rightSlotW = frameW - leftSlotW;
        }

        uploadPageTexture(vg, leftPage_, leftTex_, leftW_, leftH_, leftPageCached_,
                          leftSlotTargetW_, leftSlotTargetH_, leftSlotW, frameH);
        if (spread)
            uploadPageTexture(vg, rightPage_, rightTex_, rightW_, rightH_, rightPageCached_,
                              rightSlotTargetW_, rightSlotTargetH_, rightSlotW, frameH);

        else if (rightTex_ != 0) {

            nvgDeleteImage(vg, rightTex_);

            rightTex_ = 0;

            rightPageCached_ = -1;

            rightW_ = rightH_ = 0;

        }

    }



    static void drawPageTexture(NVGcontext* vg, float dx, float dy, float dw, float dh, int tex,

                                float alpha)

    {

        if (tex == 0 || alpha <= 0.f || dw <= 0.f || dh <= 0.f)

            return;

        NVGpaint paint = nvgImagePattern(vg, dx, dy, dw, dh, 0, tex, alpha);

        nvgBeginPath(vg);

        nvgRect(vg, dx, dy, dw, dh);

        nvgFillPaint(vg, paint);

        nvgFill(vg);

    }



    static void drawPageTexture(NVGcontext* vg, const LayoutRect& layout, int tex, float alpha)

    {

        drawPageTexture(vg, layout.x, layout.y, layout.w, layout.h, tex, alpha);

    }



    void drawSpreadPagesAbs(NVGcontext* vg, float originX, float originY, float width, float height,

                            int leftTex, int rightTex, int leftW, int leftH, int rightW, int rightH,

                            bool spread)

    {

        LayoutRect leftLayout{};

        LayoutRect rightLayout{};

        if (leftTex != 0 && leftW > 0 && leftH > 0) {

            if (spread && rightTex != 0 && rightW > 0 && rightH > 0)

                layoutSpreadRects(width, height, leftW, leftH, rightW, rightH, leftLayout,

                                  rightLayout);

            else

                leftLayout = aspectFitRect(0.f, 0.f, width, height, leftW, leftH);

        }



        if (leftTex != 0 && leftLayout.w > 0.f)

            drawPageTexture(vg, originX + leftLayout.x, originY + leftLayout.y, leftLayout.w,

                            leftLayout.h, leftTex, 1.f);

        if (spread && rightTex != 0 && rightLayout.w > 0.f)

            drawPageTexture(vg, originX + rightLayout.x, originY + rightLayout.y, rightLayout.w,

                            rightLayout.h, rightTex, 1.f);

    }



    void finishSpreadTransition()

    {

        NVGcontext* vg = brls::Application::getNVGContext();

        if (vg) {

            if (outgoingLeftTex_ != 0 && outgoingLeftTex_ != leftTex_) {

                nvgDeleteImage(vg, outgoingLeftTex_);

                outgoingLeftTex_ = 0;

            }

            if (outgoingRightTex_ != 0 && outgoingRightTex_ != rightTex_) {

                nvgDeleteImage(vg, outgoingRightTex_);

                outgoingRightTex_ = 0;

            }

        }



        outgoingLeftPage_ = outgoingRightPage_ = -1;

        outgoingLeftW_ = outgoingLeftH_ = outgoingRightW_ = outgoingRightH_ = 0;

        transitionActive_ = false;

        resetViewTransform();



        if (transitionDone_) {

            auto done = std::move(transitionDone_);

            transitionDone_ = nullptr;

            done();

        }



        invalidate();

    }



    void handleControllerZoomPan()
    {
        const brls::Rect frame = getFrame();
        const auto& pad = brls::Application::getControllerState();
        const float midX = frame.getMinX() + frame.getWidth() * 0.5f;
        const float midY = frame.getMinY() + frame.getHeight() * 0.5f;

        if (pad.buttons[brls::BUTTON_RT] && !rtHeld_) {
            rtHeld_ = true;
            adjustZoom(0.2f, midX, midY, frame.getMinX(), frame.getMinY(), frame.getWidth(),
                       frame.getHeight());
        } else if (!pad.buttons[brls::BUTTON_RT]) {
            rtHeld_ = false;
        }

        if (pad.buttons[brls::BUTTON_LT] && !ltHeld_) {
            ltHeld_ = true;
            adjustZoom(-0.2f, midX, midY, frame.getMinX(), frame.getMinY(), frame.getWidth(),
                       frame.getHeight());
        } else if (!pad.buttons[brls::BUTTON_LT]) {
            ltHeld_ = false;
        }

        constexpr float kStickDeadzone = 0.25f;
        constexpr float kZoomSpeed = 0.035f;
        constexpr float kPanSpeed = 8.f;
        const float rightY = pad.axes[brls::RIGHT_Y];
        const float leftX = pad.axes[brls::LEFT_X];
        const float leftY = pad.axes[brls::LEFT_Y];

        if (std::abs(rightY) > kStickDeadzone) {
            adjustZoom(-rightY * kZoomSpeed, midX, midY, frame.getMinX(), frame.getMinY(),
                       frame.getWidth(), frame.getHeight());
        }

        if (zoom_ > 1.02f) {
            if (std::abs(leftX) > kStickDeadzone)
                panX_ -= leftX * kPanSpeed;
            if (std::abs(leftY) > kStickDeadzone)
                panY_ -= leftY * kPanSpeed;
        }
    }

    void handleTouchZoomPan()

    {

#ifdef __SWITCH__

        const brls::Rect frame = getFrame();

        HidTouchScreenState state{};

        if (!hidGetTouchScreenStates(&state, 1))

            return;



        std::vector<brls::Point> points;

        points.reserve(state.count);

        for (int i = 0; i < state.count; ++i) {

            brls::Point p;

            p.x = static_cast<float>(state.touches[i].x) / brls::Application::windowScale;

            p.y = static_cast<float>(state.touches[i].y) / brls::Application::windowScale;

            if (pointInside(frame, p))

                points.push_back(p);

        }



        if (points.size() >= 2) {

            const brls::Point mid{(points[0].x + points[1].x) * 0.5f,

                                  (points[0].y + points[1].y) * 0.5f};

            const float dist = touchDistance(points[0], points[1]);

            if (!pinchActive_) {

                pinchStartDist_ = std::max(dist, 1.f);

                pinchStartZoom_ = zoom_;

                pinchAnchor_ = mid;

                pinchActive_ = true;

            } else {

                const float newZoom =

                    std::clamp(pinchStartZoom_ * (dist / pinchStartDist_), kMinZoom, kMaxZoom);

                const float oldZoom = zoom_;

                zoom_ = newZoom;

                if (zoom_ > 1.01f && std::abs(newZoom - oldZoom) > 0.0001f) {

                    const float cx = frame.getMinX() + frame.getWidth() * 0.5f;

                    const float cy = frame.getMinY() + frame.getHeight() * 0.5f;

                    const float fx = pinchAnchor_.x - cx;

                    const float fy = pinchAnchor_.y - cy;

                    const float ratio = zoom_ / oldZoom;

                    panX_ = fx - (fx - panX_) * ratio;

                    panY_ = fy - (fy - panY_) * ratio;

                }

            }

            if (zoom_ <= 1.01f) {

                panX_ = 0.f;

                panY_ = 0.f;

            }

            return;

        }



        pinchActive_ = false;



        if (points.size() == 1 && zoom_ > 1.02f) {

            const brls::Point& touch = points[0];

            if (!panTouchActive_) {

                panAnchor_ = touch;

                panStartX_ = panX_;

                panStartY_ = panY_;

                panTouchActive_ = true;

            } else {

                panX_ = panStartX_ + (touch.x - panAnchor_.x);

                panY_ = panStartY_ + (touch.y - panAnchor_.y);

            }

        } else {

            panTouchActive_ = false;

        }

#else

        (void)pinchActive_;

#endif

    }



    PdfManual* pdf_ = nullptr;

    int leftPage_ = -1;

    int rightPage_ = -1;



    int leftTex_ = 0;

    int rightTex_ = 0;

    int leftW_ = 0;

    int leftH_ = 0;

    int rightW_ = 0;

    int rightH_ = 0;

    int leftPageCached_ = -1;

    int rightPageCached_ = -1;

    int leftSlotTargetW_ = 0;

    int leftSlotTargetH_ = 0;

    int rightSlotTargetW_ = 0;

    int rightSlotTargetH_ = 0;



    float zoom_ = 1.f;

    float panX_ = 0.f;

    float panY_ = 0.f;

    bool pinchActive_ = false;

    bool panTouchActive_ = false;

    float pinchStartDist_ = 1.f;

    float pinchStartZoom_ = 1.f;

    brls::Point pinchAnchor_{};

    brls::Point panAnchor_{};

    float panStartX_ = 0.f;

    float panStartY_ = 0.f;



    bool rtHeld_ = false;
    bool ltHeld_ = false;

    std::function<void()> onSwipePrev_;

    std::function<void()> onSwipeNext_;



    bool transitionActive_ = false;

    int transitionDir_ = 0;

    brls::Animatable transitionAnim_{0.f};

    std::function<void()> transitionDone_;



    int outgoingLeftPage_ = -1;

    int outgoingRightPage_ = -1;

    int outgoingLeftTex_ = 0;

    int outgoingRightTex_ = 0;

    int outgoingLeftW_ = 0;

    int outgoingLeftH_ = 0;

    int outgoingRightW_ = 0;

    int outgoingRightH_ = 0;

};



ManualViewerView::ManualViewerView(std::string pdfPath, std::string title)

    : pdf_(std::make_unique<PdfManual>())

    , title_(std::move(title))

    , layoutMode_(Config::instance().manualLayoutMode() == "single_page"
                      ? ManualLayoutMode::SinglePage
                      : ManualLayoutMode::CoverThenSpread)

{

    auto& theme = ThemeManager::instance();

    setAxis(brls::Axis::COLUMN);

    setPadding(16, 20, 20, 20);

    setBackgroundColor(theme.color("brls/background"));

    setCornerRadius(0);

    setBorderThickness(0);

    setFocusable(true);

    setHideHighlight(true);



    if (!pdf_->open(pdfPath)) {

        SF_LOG_W("Manual", "Could not open PDF: %s", pdfPath.c_str());

    }



    auto* header = new brls::Label();

    header->setText(title_.empty() ? "Game Manual" : title_);

    header->setFontSize(24);

    header->setTextColor(theme.color("nxstation/title_text"));

    header->setMarginBottom(8);

    addView(header);



    canvas_ = new ManualPageCanvas();

    canvas_->setDocument(pdf_.get());

    canvas_->setOnSwipe([this] { prevSpread(); }, [this] { nextSpread(); });

    addView(canvas_);



    pageLabel_ = new brls::Label();

    pageLabel_->setFontSize(20);

    pageLabel_->setMarginTop(10);

    pageLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);

    pageLabel_->setTextColor(theme.color("nxstation/detail_text"));

    addView(pageLabel_);



    layoutHint_ = new brls::Label();

    layoutHint_->setFontSize(16);

    layoutHint_->setMarginTop(6);

    layoutHint_->setHorizontalAlign(brls::HorizontalAlign::CENTER);

    layoutHint_->setTextColor(theme.color("nxstation/detail_text"));

    addView(layoutHint_);



    refreshPageLabel();

    if (pageCount() > 0)

        showSpread(0);



    registerNavigationActions();

}



ManualViewerView::~ManualViewerView()
{
    shutdown();
}

void ManualViewerView::shutdown()
{
    if (shutdown_)
        return;
    shutdown_ = true;

    if (canvas_) {
        if (NVGcontext* vg = brls::Application::getNVGContext())
            canvas_->releaseGpuResources(vg);
        canvas_->setDocument(nullptr);
    }

    if (pdf_)
        pdf_->close();
}



size_t ManualViewerView::pageCount() const

{

    return pdf_ && pdf_->isOpen() ? static_cast<size_t>(pdf_->pageCount()) : 0;

}



size_t ManualViewerView::maxSpreadIndex() const

{

    const size_t pages = pageCount();

    if (pages == 0)

        return 0;

    if (layoutMode_ == ManualLayoutMode::SinglePage)

        return pages - 1;

    if (pages == 1)

        return 0;

    return 1 + (pages - 2) / 2;

}



void ManualViewerView::getSpreadPages(size_t spreadIndex, int& leftPage, int& rightPage) const

{

    leftPage = -1;

    rightPage = -1;

    const size_t pages = pageCount();

    if (pages == 0)

        return;



    if (layoutMode_ == ManualLayoutMode::SinglePage) {

        leftPage = static_cast<int>(spreadIndex);

        return;

    }



    if (spreadIndex == 0) {

        leftPage = 0;

        return;

    }



    leftPage = static_cast<int>(spreadIndex * 2 - 1);

    const int candidate = leftPage + 1;

    if (candidate < static_cast<int>(pages))

        rightPage = candidate;

}



void ManualViewerView::showSpread(size_t spreadIndex)

{

    if (pageCount() == 0 || !canvas_)

        return;



    spreadIndex = std::min(spreadIndex, maxSpreadIndex());

    spreadIndex_ = spreadIndex;



    int left = -1;

    int right = -1;

    getSpreadPages(spreadIndex_, left, right);

    canvas_->setSpread(left, right);

    refreshPageLabel();

}



void ManualViewerView::refreshPageLabel()

{

    if (!pageLabel_ || !layoutHint_)

        return;



    int left = -1;

    int right = -1;

    getSpreadPages(spreadIndex_, left, right);

    const size_t pages = pageCount();



    char buf[96];

    if (right >= 0)

        std::snprintf(buf, sizeof(buf), "Pages %d–%d of %zu", left + 1, right + 1, pages);

    else if (left >= 0)

        std::snprintf(buf, sizeof(buf), "Page %d of %zu", left + 1, pages);

    else

        std::snprintf(buf, sizeof(buf), "No pages");

    pageLabel_->setText(buf);



    layoutHint_->setText(

        layoutMode_ == ManualLayoutMode::CoverThenSpread

            ? "Cover + Spread  ·  D-pad/L/R pages  ·  R-stick zoom  ·  L-stick pan  ·  Y Options  ·  B Back"

            : "Single page  ·  D-pad/L/R pages  ·  R-stick zoom  ·  L-stick pan  ·  Y Options  ·  B Back");

}



void ManualViewerView::nextSpread()

{

    if (spreadIndex_ >= maxSpreadIndex())

        return;



    const size_t next = spreadIndex_ + 1;

    int left = -1;

    int right = -1;

    getSpreadPages(next, left, right);

    playNavSfx();



    // Manual spreads always slide horizontally.

    if (!canvas_->beginSpreadTransition(left, right, +1, [this, next]() {

            spreadIndex_ = next;

            refreshPageLabel();

        })) {

        showSpread(next);

    }

}



void ManualViewerView::prevSpread()

{

    if (spreadIndex_ == 0)

        return;



    const size_t prev = spreadIndex_ - 1;

    int left = -1;

    int right = -1;

    getSpreadPages(prev, left, right);

    playNavSfx();



    if (!canvas_->beginSpreadTransition(left, right, -1, [this, prev]() {

            spreadIndex_ = prev;

            refreshPageLabel();

        })) {

        showSpread(prev);

    }

}



void ManualViewerView::openLayoutMenu()

{

    playConfirmSfx();



    auto* panel = new brls::Box();

    stylePopupMenuPanel(panel);



    auto* header = new brls::Header();

    header->setTitle("Manual Viewer");

    header->setSubtitle("Layout mode");

    panel->addView(header);



    auto* row = new brls::DetailCell();

    row->setText("Cover + Spread");

    row->setDetailText(layoutMode_ == ManualLayoutMode::CoverThenSpread ? "On" : "Off");

    stylePopupMenuToggleRow(row);

    row->registerClickAction([this, row](brls::View*) {

        playToggleSfx();

        layoutMode_ = layoutMode_ == ManualLayoutMode::CoverThenSpread
                          ? ManualLayoutMode::SinglePage
                          : ManualLayoutMode::CoverThenSpread;

        row->setDetailText(layoutMode_ == ManualLayoutMode::CoverThenSpread ? "On" : "Off");

        Config::instance().setManualLayoutMode(
            layoutMode_ == ManualLayoutMode::SinglePage ? "single_page" : "cover_spread");
        Config::instance().saveUserSettings();

        if (spreadIndex_ > maxSpreadIndex())

            spreadIndex_ = maxSpreadIndex();

        showSpread(spreadIndex_);

        return true;

    });

    panel->addView(row);



    auto* note = new brls::Label();

    note->setText("Cover + Spread: page 1 alone, then two-page spreads.");

    note->setFontSize(18);

    note->setSingleLine(false);

    note->setMarginTop(8);

    note->setTextColor(ThemeManager::instance().color("nxstation/detail_text"));

    panel->addView(note);



  FocusedMenuDialog::present(panel);

}



void ManualViewerView::registerNavigationActions()

{

    auto prev = [this](brls::View*) {

        prevSpread();

        return true;

    };

    auto next = [this](brls::View*) {

        nextSpread();

        return true;

    };



    registerAction("Previous", brls::ControllerButton::BUTTON_LEFT, prev);

    registerAction("Previous", brls::ControllerButton::BUTTON_LB, prev);

    registerAction("Next", brls::ControllerButton::BUTTON_RIGHT, next);

    registerAction("Next", brls::ControllerButton::BUTTON_RB, next);



    auto consumeStickNav = [](brls::View*) { return true; };
    registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, consumeStickNav);
    registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, consumeStickNav);
    registerAction("", brls::ControllerButton::BUTTON_NAV_UP, consumeStickNav);
    registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, consumeStickNav);



    registerAction("Zoom In", brls::ControllerButton::BUTTON_RT, [this](brls::View*) {

        if (!canvas_)

            return true;

        const brls::Rect frame = canvas_->getFrame();
        const float midX = frame.getMinX() + frame.getWidth() * 0.5f;
        const float midY = frame.getMinY() + frame.getHeight() * 0.5f;
        canvas_->adjustZoom(0.15f, midX, midY, frame.getMinX(),

                            frame.getMinY(), frame.getWidth(), frame.getHeight());

        return true;

    });

    registerAction("Zoom Out", brls::ControllerButton::BUTTON_LT, [this](brls::View*) {

        if (!canvas_)

            return true;

        const brls::Rect frame = canvas_->getFrame();
        const float midX = frame.getMinX() + frame.getWidth() * 0.5f;
        const float midY = frame.getMinY() + frame.getHeight() * 0.5f;
        canvas_->adjustZoom(-0.15f, midX, midY, frame.getMinX(),

                            frame.getMinY(), frame.getWidth(), frame.getHeight());

        return true;

    });



    registerAction("Options", brls::ControllerButton::BUTTON_Y, [this](brls::View*) {

        openLayoutMenu();

        return true;

    });

}



void ManualViewerView::present(std::string pdfPath, std::string title,
                               std::function<void()> onDismiss)

{

    if (pdfPath.empty())

        return;



    auto* view = new ManualViewerView(std::move(pdfPath), std::move(title));
    view->setWidth(kFrameW);
    view->setHeight(kFrameH);
    view->setGrow(1.f);

    auto dismissHook = [view]() { view->shutdown(); };
    auto* dialog = FocusedMenuDialog::presentFullscreen(view, dismissHook);
    view->dialog_ = dialog;

    auto finishSession = [onDismiss = std::move(onDismiss)]() {
        if (onDismiss)
            onDismiss();
    };

    auto closeViewer = [dialog, finishSession]() {
        if (!dialog)
            return;
        dialog->close(finishSession);
    };

    view->registerAction(
        "Back", brls::ControllerButton::BUTTON_B,
        [closeViewer](brls::View*) {
            closeViewer();
            return true;
        });

    if (auto* applet = dialog->getAppletFrame()) {
        applet->registerAction(
            "Back", brls::ControllerButton::BUTTON_B,
            [closeViewer](brls::View*) {
                closeViewer();
                return true;
            });
    }

    brls::Application::giveFocus(view->canvas_);

}



} // namespace sf::ui

