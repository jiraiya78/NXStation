#include "ui/ScrapeMenuView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "ui/UiSfx.hpp"

#include "app/Config.hpp"
#include "util/ActionLog.hpp"

#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <memory>

namespace sf::ui {

ScrapeMenuView::ScrapeMenuView(std::string systemId, ScrapeMenuCallback onSelect)
    : systemId_(std::move(systemId))
    , onSelect_(std::move(onSelect))
{
    stylePopupMenuPanel(this);

    const SystemConfig* sys = Config::instance().findSystem(systemId_);
    auto* header = new brls::Header();
    header->setTitle("Scrape Library");
    header->setSubtitle(sys ? sys->name : systemId_);
    this->addView(header);

    auto menuScroller = makePopupMenuScroller(460.f);
    brls::ScrollingFrame* scroller = menuScroller.scroller;

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);

    auto* assetsHeader = new brls::Header();
    assetsHeader->setTitle("Assets to Download");
    assetsHeader->setSubtitle("Toggle before starting a scrape");
    list->addView(assetsHeader);

    auto addToggle = [&](const char* title, bool initial, auto setter) {
        auto* row = new brls::DetailCell();
        row->setText(title);
        stylePopupMenuToggleRow(row);
        auto state = std::make_shared<bool>(initial);
        row->setDetailText(*state ? "On" : "Off");
        row->registerClickAction([row, setter, state](brls::View*) {
            playToggleSfx();
            *state = !*state;
            setter(*state);
            Config::instance().saveUserSettings();
            row->setDetailText(*state ? "On" : "Off");
            return true;
        });
        list->addView(row);
    };

    auto& cfg = Config::instance();
    addToggle("Box Art", cfg.scrapeBoxArt(),
              [&](bool v) { cfg.setScrapeBoxArt(v); });
    addToggle("Thumbnail", cfg.scrapeThumbnail(),
              [&](bool v) { cfg.setScrapeThumbnail(v); });
    addToggle("Video", cfg.scrapeVideo(),
              [&](bool v) { cfg.setScrapeVideo(v); });
    addToggle("Manual", cfg.scrapeManual(),
              [&](bool v) { cfg.setScrapeManual(v); });
    addToggle("Optimized Media", cfg.scrapeOptimizedMedia(),
              [&](bool v) { cfg.setScrapeOptimizedMedia(v); });

    auto addOption = [&](const char* title, ScrapeMode mode) {
        auto* row = new brls::DetailCell();
        row->setText(title);
        stylePopupMenuRow(row);
        row->registerClickAction([this, mode](brls::View*) {
            SF_LOG_ACTION("Scrape/MenuSelect");
            auto callback = onSelect_;
            if (dialog_) {
                dialog_->close([callback, mode] {
                    if (callback)
                        callback(mode);
                });
            } else if (callback) {
                callback(mode);
            }
            return true;
        });
        list->addView(row);
    };

    addOption("Scrape Individual Game", ScrapeMode::Single);
    addOption("Scrape All Games", ScrapeMode::Full);
    addOption("Scrape Missing Art", ScrapeMode::MissingArtOnly);

    scroller->setContentView(list);
    this->addView(menuScroller.clipBox);
    registerPopupPageActions(this, scroller);
}

void ScrapeMenuView::present(const std::string& systemId, ScrapeMenuCallback onSelect,
                             std::function<void()> onDismiss)
{
    auto* menu = new ScrapeMenuView(systemId, std::move(onSelect));
    auto* dialog = FocusedMenuDialog::present(menu, std::move(onDismiss));
    menu->dialog_ = dialog;
}

} // namespace sf::ui
