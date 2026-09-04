#pragma once

#include "app/Models.hpp"

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>

namespace sf::ui {

class VideoPreviewView;

/** Full-screen random game video screensaver. */
class ScreensaverView : public brls::Box {
public:
    ScreensaverView();
    ~ScreensaverView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    void setOnDismiss(std::function<void()> cb) { onDismiss_ = std::move(cb); }

    static void present(std::function<void()> onDismiss = nullptr);

    /** True only while the screensaver is on screen. */
    static bool isShowing();

    /** True while showing, and for a short cooldown after dismissal. */
    static bool isActive();

    void launchCurrent();
    void dismiss();

private:
    struct Pick {
        std::string systemId;
        sf::GameItem game;
    };

    void pickRandomGame();
    void refillShuffleBag();
    void showCurrent();
    void tickPlayback(float delta);
    void applyThemeColors();

    Pick current_{};
    bool hasPick_ = false;
    std::vector<Pick> shuffleBag_;
    size_t shuffleIndex_ = 0;
    bool wasPlaying_ = false;
    float sinceSelection_ = 0.f;
    std::string lastPath_;
    brls::Time lastFrameTime_ = 0;

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::function<void()> onDismiss_;

    VideoPreviewView* videoPreview_ = nullptr;
    brls::Image* backgroundImage_ = nullptr;
    brls::Box* overlayBox_ = nullptr;
    brls::Label* titleLabel_ = nullptr;
    brls::Label* systemLabel_ = nullptr;
    brls::Label* hintLabel_ = nullptr;
    float savedHoverDelay_ = 1.0f;
    bool savedLoopPlayback_ = true;
    uint32_t themeGenApplied_ = 0;
    std::string screensaverImagePath_;
};

/** Activity for screensaver — A launches previewed game, any other input dismisses. */
class ScreensaverActivity : public brls::Activity {
public:
    explicit ScreensaverActivity(ScreensaverView* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onContentAvailable() override;

private:
    ScreensaverView* view_ = nullptr;
};

} // namespace sf::ui
