#pragma once

#include <borealis.hpp>
#include <cmath>

namespace sf::ui {

/** Tracks controller idle time for screensaver activation. */
class IdleTimer {
public:
    void reset() { lastInputUs_ = brls::getCPUTimeUsec(); }

    void pollController()
    {
        const auto& pad = brls::Application::getControllerState();
        for (size_t i = 0; i < brls::_BUTTON_MAX; ++i) {
            if (pad.buttons[i]) {
                reset();
                return;
            }
        }
        for (float axis : pad.axes) {
            if (std::abs(axis) > 0.25f) {
                reset();
                return;
            }
        }
    }

    bool idleSeconds(float seconds) const
    {
        if (seconds <= 0.f)
            return false;
        const brls::Time now = brls::getCPUTimeUsec();
        return static_cast<double>(now - lastInputUs_) / 1000000.0
               >= static_cast<double>(seconds);
    }

private:
    brls::Time lastInputUs_ = brls::getCPUTimeUsec();
};

} // namespace sf::ui
