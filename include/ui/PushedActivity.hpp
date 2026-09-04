#pragma once

#include <borealis/core/i18n.hpp>
#include <borealis.hpp>

using namespace brls::literals;

namespace sf::ui {

/** Activity pushed on top of the main tab; B goes back (pop), not quit. */
class PushedActivity : public brls::Activity {
public:
    explicit PushedActivity(brls::View* view)
        : brls::Activity(view)
    {
    }

    static void push(brls::View* view)
    {
        brls::Application::pushActivity(new PushedActivity(view),
                                        brls::TransitionAnimation::FADE);
    }

    static void push(brls::Activity* activity)
    {
        brls::Application::pushActivity(activity, brls::TransitionAnimation::FADE);
    }

    void onContentAvailable() override
    {
        if (!getContentView())
            return;

        getContentView()->registerAction(
            "hints/back"_i18n, brls::BUTTON_B,
            [](brls::View*) {
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return true;
            },
            false, false, brls::SOUND_BACK);
    }
};

} // namespace sf::ui
