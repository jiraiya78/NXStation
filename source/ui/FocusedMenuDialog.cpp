#include "ui/FocusedMenuDialog.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"

#include <borealis/core/application.hpp>
#include <algorithm>
#include <nanovg.h>

namespace sf::ui {

namespace {

constexpr int kDialogOpenMs = 180;
constexpr int kDialogCloseMs = 120;

} // namespace

void stylePopupMenuPanel(brls::Box* panel)
{
    auto& theme = ThemeManager::instance();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setPadding(20, 28, 28, 28);
    panel->setBackgroundColor(theme.color("nxstation/dialog_bg"));
    panel->setCornerRadius(14);
    panel->setBorderColor(theme.color("nxstation/dialog_border"));
    panel->setBorderThickness(3);
    panel->setShadowType(brls::ShadowType::GENERIC);
    panel->setClipsToBounds(true);
}

PopupMenuScroller makePopupMenuScroller(float height)
{
    PopupMenuScroller out;
    out.clipBox = new brls::Box();
    out.clipBox->setAxis(brls::Axis::COLUMN);
    out.clipBox->setHeight(height);
    out.clipBox->setGrow(1.f);
    out.clipBox->setClipsToBounds(true);

    out.scroller = new brls::ScrollingFrame();
    out.scroller->setHeight(height);
    out.scroller->setGrow(1.f);
    out.scroller->setScrollingIndicatorVisible(false);
    out.scroller->setClipsToBounds(true);

    out.clipBox->addView(out.scroller);
    return out;
}

void stylePopupMenuRow(brls::DetailCell* row)
{
    auto& theme = ThemeManager::instance();
    row->detail->setVisibility(brls::Visibility::GONE);
    row->setHeight(64);
    row->setLineBottom(0);
    row->setTextColor(theme.color("nxstation/title_text"));
    row->setBackgroundColor(theme.color("nxstation/dialog_row"));
}

void stylePopupMenuToggleRow(brls::DetailCell* row)
{
    auto& theme = ThemeManager::instance();
    row->detail->setVisibility(brls::Visibility::VISIBLE);
    row->setHeight(64);
    row->setLineBottom(0);
    row->setTextColor(theme.color("nxstation/title_text"));
    row->setDetailTextColor(theme.color("nxstation/detail_text"));
    row->setBackgroundColor(theme.color("nxstation/dialog_row"));
}

void registerPopupPageActions(brls::View* host, brls::ScrollingFrame* scroller)
{
    if (!host || !scroller)
        return;

    auto page = [scroller](int direction) {
        playNavSfx();
        const float viewH = scroller->getHeight();
        const float pageSize = viewH > 0.f ? viewH * 0.85f : 360.f;
        float offset = scroller->getContentOffsetY();
        if (direction < 0)
            offset = std::max(0.f, offset - pageSize);
        else
            offset += pageSize;
        scroller->setContentOffsetY(offset, false);
        return true;
    };

    host->registerAction("Page Up", brls::ControllerButton::BUTTON_LB,
                         [page](brls::View*) { return page(-1); });
    host->registerAction("Page Down", brls::ControllerButton::BUTTON_RB,
                         [page](brls::View*) { return page(1); });
}

FocusedMenuDialog::FocusedMenuDialog(brls::Box* content, bool opaqueFullscreen)
    : brls::Dialog(content)
    , opaqueFullscreen_(opaqueFullscreen)
{
    auto& theme = ThemeManager::instance();
    const auto bg = theme.color("brls/background");

    if (opaqueFullscreen_) {
        setBackgroundColor(bg);
        setPadding(0, 0, 0, 0);
        setJustifyContent(brls::JustifyContent::FLEX_START);
        setAlignItems(brls::AlignItems::STRETCH);
        setWidth(brls::Application::contentWidth);
        setHeight(brls::Application::contentHeight);

        if (auto* applet = getAppletFrame()) {
            applet->setWidth(brls::Application::contentWidth);
            applet->setHeight(brls::Application::contentHeight);
            applet->setCornerRadius(0);
            applet->setBackgroundColor(bg);
        }
    } else {
        setBackground(brls::ViewBackground::NONE);
        if (auto* applet = getAppletFrame())
            applet->setBackgroundColor(theme.color("nxstation/dialog_backdrop"));
    }
}

bool FocusedMenuDialog::isTranslucent()
{
    return !opaqueFullscreen_;
}

void FocusedMenuDialog::willAppear(bool resetState)
{
    brls::Dialog::willAppear(resetState);

    closing_ = false;
    scaleAnim_.stop();
    scaleAnim_.reset(0.95f);
    scaleAnim_.addStep(1.f, kDialogOpenMs, brls::EasingFunction::quadraticOut);
    scaleAnim_.setTickCallback([this]() { this->invalidate(); });
    scaleAnim_.start();

    // Soft fade-in. Fullscreen stays opaque enough that the list never shows through.
    alpha.stop();
    alpha.reset(opaqueFullscreen_ ? 1.f : 0.f);
    if (!opaqueFullscreen_) {
        alpha.addStep(1.f, kDialogOpenMs, brls::EasingFunction::quadraticOut);
        alpha.setTickCallback([this]() { this->invalidate(); });
        alpha.start();
    }
}

void FocusedMenuDialog::draw(NVGcontext* vg, float x, float y, float width, float height,
                             brls::Style style, brls::FrameContext* ctx)
{
    const float scale = scaleAnim_.getValue();
    nvgSave(vg);
    nvgTranslate(vg, x + width * 0.5f, y + height * 0.5f);
    nvgScale(vg, scale, scale);
    nvgTranslate(vg, -(x + width * 0.5f), -(y + height * 0.5f));
    brls::Dialog::draw(vg, x, y, width, height, style, ctx);
    nvgRestore(vg);
}

FocusedMenuDialog* FocusedMenuDialog::present(brls::Box* content, std::function<void()> onDismiss)
{
    auto* dialog = new FocusedMenuDialog(content);
    dialog->onDismiss_ = std::move(onDismiss);
    dialog->setCancelable(true);
    dialog->brls::Dialog::open();
    return dialog;
}

FocusedMenuDialog* FocusedMenuDialog::presentFullscreen(brls::Box* content,
                                                        std::function<void()> onDismiss)
{
    auto* dialog = new FocusedMenuDialog(content, true);
    dialog->onDismiss_ = std::move(onDismiss);
    dialog->setCancelable(true);
    dialog->brls::Dialog::open();
    return dialog;
}

void FocusedMenuDialog::startCloseAnimation(std::function<void(void)> cb)
{
    if (closing_) {
        cb();
        return;
    }
    closing_ = true;

    // Scale the panel down slightly. Do NOT drive dialog alpha to 0 before pop —
    // that reveals the list underneath (fullscreen) and Borealis FADE would also
    // reset alpha to 1 and flash. We pop with NONE after this short tween.
    scaleAnim_.stop();
    scaleAnim_.reset(scaleAnim_.getValue());
    scaleAnim_.addStep(0.96f, kDialogCloseMs, brls::EasingFunction::quadraticIn);
    scaleAnim_.setTickCallback([this]() { this->invalidate(); });

    if (opaqueFullscreen_) {
        scaleAnim_.setEndCallback([this, cb = std::move(cb)](bool finished) {
            if (finished)
                finishDismiss(std::move(cb));
            else
                closing_ = false;
        });
        scaleAnim_.start();
        return;
    }

    // Translucent menus: gentle fade of the whole dialog, then instant pop.
    alpha.stop();
    alpha.reset(alpha.getValue());
    alpha.addStep(0.f, kDialogCloseMs, brls::EasingFunction::quadraticIn);
    alpha.setTickCallback([this]() { this->invalidate(); });
    alpha.setEndCallback([this, cb = std::move(cb)](bool finished) {
        if (finished)
            finishDismiss(std::move(cb));
        else
            closing_ = false;
    });
    scaleAnim_.start();
    alpha.start();
}

void FocusedMenuDialog::finishDismiss(std::function<void(void)> cb)
{
    auto onDismiss = std::move(onDismiss_);
    onDismiss_ = nullptr;
    // NONE avoids Borealis hide() resetting alpha to 1 (the close flicker).
    brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                   [onDismiss = std::move(onDismiss), cb = std::move(cb)]() {
                                       if (onDismiss)
                                           onDismiss();
                                       cb();
                                   });
}

void FocusedMenuDialog::dismiss(std::function<void(void)> cb)
{
    if (closing_) {
        cb();
        return;
    }
    startCloseAnimation(std::move(cb));
}

void FocusedMenuDialog::close(std::function<void(void)> cb)
{
    dismiss(std::move(cb));
}

} // namespace sf::ui
