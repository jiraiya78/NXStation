#pragma once

#include "app/Models.hpp"

#include <borealis.hpp>
#include <functional>
#include <string>

namespace sf::ui {

class AspectFitImage;
class VideoPreviewView;
class FocusedMenuDialog;

class GameDetailView : public brls::Box {
public:
    GameDetailView(sf::GameItem game, sf::SystemConfig system, bool metadataOnly = false);
    ~GameDetailView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    /** Metadata popup (slightly inset dialog) over the game list. */
    static void presentMetadata(const sf::GameItem& game, const sf::SystemConfig& system,
                                std::function<void()> onDismiss = nullptr);

private:
    void launchGame();
    void scrapeNow();
    void refreshUi();
    void installVideoPreview();
    void addMetadataActions();
    void tickDescriptionStickScroll(float delta);

    sf::GameItem game_;
    sf::SystemConfig system_;
    bool metadataOnly_ = false;
    FocusedMenuDialog* dialog_ = nullptr;
    std::function<void(const std::string& oldPath, const std::string& newPath)> onRenamed_;
    std::function<void(const std::string& path)> onDeleted_;

    BRLS_BIND(brls::Label, titleLabel, "detail/title");
    BRLS_BIND(brls::Label, metaLabel, "detail/meta");
    BRLS_BIND(brls::Label, descLabel, "detail/desc");
    BRLS_BIND(brls::Image, boxArt, "detail/boxart");
    BRLS_BIND(brls::Box, boxArtFrame, "detail/boxart_frame");
    BRLS_BIND(brls::Image, thumbArt, "detail/thumb");
    BRLS_BIND(brls::Box, thumbFrame, "detail/thumb_frame");
    BRLS_BIND(brls::Box, videoSlot, "detail/video");
    BRLS_BIND(brls::Button, launchBtn, "detail/launch");
    BRLS_BIND(brls::Button, scrapeBtn, "detail/scrape");
    BRLS_BIND(brls::Button, raMenuBtn, "detail/ramenu");
    BRLS_BIND(brls::Label, filenameLabel, "detail/filename");
    BRLS_BIND(brls::Box, metaActions, "detail/meta_actions");
    BRLS_BIND(brls::ScrollingFrame, descScroll, "detail/desc_scroll");

    VideoPreviewView* videoPreview_ = nullptr;
    AspectFitImage* aspectBoxArt_ = nullptr;
    AspectFitImage* aspectThumbArt_ = nullptr;
    brls::Button* firstMetaBtn_ = nullptr;
    brls::Time lastFrameTime_ = 0;
};

} // namespace sf::ui
