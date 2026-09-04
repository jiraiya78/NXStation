#include "ui/LaunchTransition.hpp"

#include "app/AppState.hpp"
#include "launcher/NroLauncher.hpp"
#include "ui/AspectFitImage.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#include <borealis.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace sf::ui {
namespace {

std::atomic<bool> gLaunchInFlight{false};

class LaunchTransitionView : public brls::Box {
public:
    LaunchTransitionView(SystemConfig system, GameItem game)
        : system_(std::move(system))
        , game_(std::move(game))
    {
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setBackgroundColor(nvgRGB(0, 0, 0));
        this->setFocusable(true);
        this->setHideHighlight(true);

        art_ = new AspectFitImage();
        art_->setWidth(kBaseArtSize);
        art_->setHeight(kBaseArtSize);

        std::string artPath = game_.meta.boxArtPath;
        if (artPath.empty() || !FileSystem::exists(artPath)) {
            const std::string bundled = "romfs:/img/systems/" + system_.id + ".png";
            if (FileSystem::exists(bundled))
                artPath = bundled;
        }
        if (!artPath.empty() && FileSystem::exists(artPath))
            art_->setImageFromFile(artPath);
        else
            art_->setImageFromRes("img/placeholder.png");

        this->addView(art_);
        this->setAlpha(1.f);
    }

    void willAppear(bool resetState) override
    {
        brls::Box::willAppear(resetState);
        brls::Application::giveFocus(this);
        startAnim();
    }

private:
    static constexpr float kBaseArtSize = 520.f;

    void startAnim()
    {
        constexpr int kDurationMs = 1000;

        scaleAnim_.stop();
        scaleAnim_.reset(1.0f);
        scaleAnim_.addStep(1.45f, kDurationMs, brls::EasingFunction::quadraticIn);

        fadeAnim_.stop();
        fadeAnim_.reset(1.0f);
        fadeAnim_.addStep(0.0f, kDurationMs, brls::EasingFunction::quadraticIn);
        fadeAnim_.setTickCallback([this]() {
            const float scale = scaleAnim_.getValue();
            const float size = kBaseArtSize * scale;
            art_->setWidth(size);
            art_->setHeight(size);
            this->setAlpha(fadeAnim_.getValue());
            this->invalidate();
        });
        fadeAnim_.setEndCallback([this](bool finished) {
            if (!finished) {
                gLaunchInFlight.store(false);
                return;
            }
            finishLaunch();
        });

        scaleAnim_.start();
        fadeAnim_.start();
    }

    void finishLaunch()
    {
        std::string error;
        if (!NroLauncher::launch(system_, game_.path, error)) {
            gLaunchInFlight.store(false);
            brls::Application::notify(error.empty() ? "Launch failed" : error);
            brls::Application::popActivity(brls::TransitionAnimation::NONE);
        }
        // On success the app quits via chain-load; leave gLaunchInFlight set.
    }

    SystemConfig system_;
    GameItem game_;
    AspectFitImage* art_ = nullptr;
    brls::Animatable scaleAnim_{1.f};
    brls::Animatable fadeAnim_{1.f};
};

} // namespace

void beginGameLaunch(const SystemConfig& system, const GameItem& game)
{
    bool expected = false;
    if (!gLaunchInFlight.compare_exchange_strong(expected, true))
        return;

    SF_LOG_I("UI", "Launch transition: %s [%s]", game.path.c_str(), system.id.c_str());
    auto* view = new LaunchTransitionView(system, game);
    brls::Application::pushActivity(new PushedActivity(view), brls::TransitionAnimation::NONE);
}

} // namespace sf::ui
