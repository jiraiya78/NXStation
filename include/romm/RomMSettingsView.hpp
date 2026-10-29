#pragma once

#include "ui/SettingsSectionView.hpp"
#include "romm/RomMService.hpp"

#include <atomic>
#include <borealis.hpp>
#include <functional>
#include <memory>

namespace sf::ui {

/** Build RomM settings rows for SettingsSectionView. */
void buildRomMSettingsRows(brls::Box* listBox);

class RomMSyncActivity : public brls::Activity {
public:
    explicit RomMSyncActivity(brls::View* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onContentAvailable() override
    {
        if (view_)
            brls::Application::giveFocus(view_);
    }

private:
    brls::View* view_ = nullptr;
};

} // namespace sf::ui
