#pragma once

#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <borealis/core/box.hpp>

#include <functional>

namespace sf::ui {

/** Shared dimmed-backdrop dialog used for Y-menu popups. */
class FocusedMenuDialog : public brls::Dialog {
public:
    explicit FocusedMenuDialog(brls::Box* content, bool opaqueFullscreen = false);

    static FocusedMenuDialog* present(brls::Box* content, std::function<void()> onDismiss = nullptr);

    /** Full-screen opaque dialog (manual viewer) — blocks dimmed backdrop underneath. */
    static FocusedMenuDialog* presentFullscreen(brls::Box* content,
                                                std::function<void()> onDismiss = nullptr);

    bool isTranslucent() override;

    void willAppear(bool resetState = false) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void dismiss(std::function<void(void)> cb = [] {}) override;
    void close(std::function<void(void)> cb = [] {}) override;

private:
    void startCloseAnimation(std::function<void(void)> cb);
    void finishDismiss(std::function<void(void)> cb);

    std::function<void()> onDismiss_;
    bool opaqueFullscreen_ = false;
    bool closing_ = false;
    brls::Animatable scaleAnim_{1.f};
};

/** Style a popup menu panel (Game Options, Scrape menu, etc.). */
void stylePopupMenuPanel(brls::Box* panel);

/** Style a single menu row inside a popup panel. */
void stylePopupMenuRow(brls::DetailCell* row);

/** Popup row with visible On/Off detail text (scrape toggles). */
void stylePopupMenuToggleRow(brls::DetailCell* row);

/** Register L/R shoulder paging on a popup that hosts a ScrollingFrame. */
void registerPopupPageActions(brls::View* host, brls::ScrollingFrame* scroller);

/** Clipped scroller host for floating menus (no scrollbar, no bleed during L/R paging). */
struct PopupMenuScroller {
    brls::Box* clipBox = nullptr;
    brls::ScrollingFrame* scroller = nullptr;
};

PopupMenuScroller makePopupMenuScroller(float height = 420.f);

} // namespace sf::ui
