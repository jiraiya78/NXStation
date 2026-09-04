#include "ui/AnalyticsReportView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"

#ifdef USE_LIBROMFS
#include <romfs/romfs.hpp>
#endif

namespace sf::ui {

namespace {

constexpr float kFrameW = 1000.f;
constexpr float kPanelW = 960.f;
constexpr float kPanelH = 620.f;

void setMetricIcon(brls::Image* image, const std::string& resPath)
{
    if (!image || resPath.empty())
        return;
#ifdef USE_LIBROMFS
    if (!romfs::get(resPath).valid())
        return;
#endif
    image->setImageFromRes(resPath);
}

brls::Box* makeSectionCard(const AnalyticsSection& sec)
{
    auto& theme = ThemeManager::instance();

    auto* card = new brls::Box();
    card->setAxis(brls::Axis::ROW);
    card->setPadding(16, 16, 16, 16);
    card->setMarginBottom(12);
    card->setCornerRadius(10);
    card->setBackgroundColor(theme.color("nxstation/dialog_row"));
    card->setAlignItems(brls::AlignItems::FLEX_START);

    if (!sec.iconRes.empty()) {
        auto* icon = new brls::Image();
        icon->setWidth(56);
        icon->setHeight(56);
        icon->setScalingType(brls::ImageScalingType::FIT);
        icon->setImageAlign(brls::ImageAlignment::CENTER);
        icon->setMarginRight(14);
        setMetricIcon(icon, sec.iconRes);
        card->addView(icon);
    }

    auto* textCol = new brls::Box();
    textCol->setAxis(brls::Axis::COLUMN);
    textCol->setGrow(1.f);

    auto* title = new brls::Label();
    title->setText(sec.title);
    title->setFontSize(26);
    title->setTextColor(theme.color("nxstation/title_text"));
    textCol->addView(title);

    auto* body = new brls::Label();
    body->setText(sec.body);
    body->setFontSize(22);
    body->setLineHeight(1.42f);
    body->setSingleLine(false);
    body->setMarginTop(6);
    body->setTextColor(theme.color("nxstation/body_text"));
    textCol->addView(body);

    card->addView(textCol);
    return card;
}

} // namespace

AnalyticsReportView::AnalyticsReportView(AnalyticsReportSpec spec)
{
    stylePopupMenuPanel(this);
    setFocusable(false);
    setHideHighlight(true);

    auto& theme = ThemeManager::instance();

    scroller_ = new brls::ScrollingFrame();
    scroller_->setGrow(1.f);
    scroller_->setScrollingIndicatorVisible(false);
    scroller_->setFocusable(true);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setPadding(4, 4, 8, 4);

    auto* windowTitle = new brls::Label();
    windowTitle->setText(spec.windowTitle);
    windowTitle->setFontSize(32);
    windowTitle->setTextColor(theme.color("nxstation/title_text"));
    windowTitle->setMarginBottom(12);
    content->addView(windowTitle);

    if (!spec.heroTitle.empty() || !spec.heroIconRes.empty()) {
        auto* hero = new brls::Box();
        hero->setAxis(brls::Axis::ROW);
        hero->setPadding(14, 14, 14, 14);
        hero->setMarginBottom(16);
        hero->setCornerRadius(12);
        hero->setBackgroundColor(theme.color("nxstation/dialog_row"));
        hero->setAlignItems(brls::AlignItems::CENTER);

        if (!spec.heroIconRes.empty()) {
            auto* heroImg = new brls::Image();
            heroImg->setWidth(96);
            heroImg->setHeight(96);
            heroImg->setScalingType(brls::ImageScalingType::FIT);
            heroImg->setImageAlign(brls::ImageAlignment::CENTER);
            heroImg->setMarginRight(18);
            setMetricIcon(heroImg, spec.heroIconRes);
            hero->addView(heroImg);
        }

        auto* heroText = new brls::Box();
        heroText->setAxis(brls::Axis::COLUMN);
        heroText->setGrow(1.f);

        if (!spec.heroTitle.empty()) {
            auto* heroTitle = new brls::Label();
            heroTitle->setText(spec.heroTitle);
            heroTitle->setFontSize(28);
            heroTitle->setSingleLine(false);
            heroTitle->setTextColor(theme.color("nxstation/title_text"));
            heroText->addView(heroTitle);
        }

        if (!spec.heroSubtitle.empty()) {
            auto* heroSub = new brls::Label();
            heroSub->setText(spec.heroSubtitle);
            heroSub->setFontSize(22);
            heroSub->setMarginTop(6);
            heroSub->setSingleLine(false);
            heroSub->setTextColor(theme.color("nxstation/detail_text"));
            heroText->addView(heroSub);
        }

        hero->addView(heroText);
        content->addView(hero);
    }

    for (const auto& section : spec.sections)
        content->addView(makeSectionCard(section));

    auto* hints = new brls::Label();
    hints->setText("B Close  ·  L/R Page");
    hints->setFontSize(18);
    hints->setMarginTop(8);
    hints->setTextColor(theme.color("nxstation/detail_text"));
    content->addView(hints);

    scroller_->setContentView(content);
    addView(scroller_);

    // Block Y-menu while this dialog is open (SystemsTab still owns the activity).
    registerAction(
        "Menu", brls::ControllerButton::BUTTON_Y, [](brls::View*) { return true; });

    registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            playNavSfx();
            const float page = scroller_->getHeight() * 0.85f;
            scroller_->setContentOffsetY(std::max(0.f, scroller_->getContentOffsetY() - page), true);
            return true;
        });
    registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            playNavSfx();
            const float page = scroller_->getHeight() * 0.85f;
            scroller_->setContentOffsetY(scroller_->getContentOffsetY() + page, true);
            return true;
        });
}

void AnalyticsReportView::present(AnalyticsReportSpec spec)
{
    playConfirmSfx();

    auto* view = new AnalyticsReportView(std::move(spec));
    view->setWidth(kPanelW);
    view->setHeight(kPanelH);

    auto* dialog = FocusedMenuDialog::present(view);
    view->dialog_ = dialog;
    if (auto* applet = dialog->getAppletFrame()) {
        applet->setWidth(kFrameW);
        applet->setCornerRadius(14);
        // SystemsTab still owns the activity; swallow Y so the menu cannot open behind us.
        applet->registerAction(
            "Menu", brls::ControllerButton::BUTTON_Y, [](brls::View*) { return true; });
    }

    brls::sync([view]() {
        if (view->scroller_)
            brls::Application::giveFocus(view->scroller_);
    });
}

} // namespace sf::ui
