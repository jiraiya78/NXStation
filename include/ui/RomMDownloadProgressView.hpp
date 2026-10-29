#pragma once

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace sf::ui {

using namespace brls::literals;

/** Download progress screen for a single RomM ROM. */
class RomMDownloadProgressView : public brls::Box {
public:
    RomMDownloadProgressView(std::string gameName, std::shared_ptr<std::atomic<bool>> abort);

    /** Main-thread only: the service posts progress through brls::sync. */
    void setStatus(const std::string& status);
    void setPercent(int percent);
    void finish(bool ok, const std::string& message);

    /** @return true if back was consumed (abort in progress). */
    bool handleBack();

    std::shared_ptr<bool> alive() const { return alive_; }
    void setOnDismiss(std::function<void()> callback) { onDismiss_ = std::move(callback); }

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;

private:
    std::string gameName_;
    std::shared_ptr<std::atomic<bool>> abort_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    bool finished_ = false;
    std::function<void()> onDismiss_;

    brls::Label* titleLabel_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::Box* progressTrack_ = nullptr;
    brls::Box* progressFill_ = nullptr;

    static constexpr float kTrackWidth = 320.f;
    int percent_ = 0;
};

/** Activity for the RomM download screen — B aborts while running, pops when finished. */
class RomMDownloadProgressActivity : public brls::Activity {
public:
    explicit RomMDownloadProgressActivity(RomMDownloadProgressView* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onContentAvailable() override
    {
        if (!view_)
            return;

        auto consume = [](brls::View*) { return true; };
        const brls::ControllerButton block[] = {
            brls::ControllerButton::BUTTON_A,
            brls::ControllerButton::BUTTON_X,
            brls::ControllerButton::BUTTON_Y,
            brls::ControllerButton::BUTTON_UP,
            brls::ControllerButton::BUTTON_DOWN,
            brls::ControllerButton::BUTTON_LEFT,
            brls::ControllerButton::BUTTON_RIGHT,
            brls::ControllerButton::BUTTON_LB,
            brls::ControllerButton::BUTTON_RB,
            brls::ControllerButton::BUTTON_START,
        };
        for (auto btn : block) {
            view_->registerAction("Block", btn, consume);
        }

        view_->registerAction(
            "hints/back"_i18n, brls::BUTTON_B,
            [this](brls::View*) {
                if (view_->handleBack())
                    return true;
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return true;
            },
            false, false, brls::SOUND_BACK);

        brls::Application::giveFocus(view_);
    }

private:
    RomMDownloadProgressView* view_ = nullptr;
};

} // namespace sf::ui
