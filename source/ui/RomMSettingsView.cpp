#include "romm/RomMSettingsView.hpp"
#include "ui/SettingsHelpers.hpp"
#include "ui/SystemsTab.hpp"
#include "ui/ThemeManager.hpp"
#include "romm/RomMService.hpp"
#include "romm/RomMConfig.hpp"
#include "app/Config.hpp"
#include "app/AppState.hpp"
#include "util/Logger.hpp"

#include <borealis.hpp>
#include <borealis/views/label.hpp>
#include <memory>
#include <string>
#include <borealis/views/rectangle.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/progress_spinner.hpp>

namespace sf::ui {

class RomMPairingView : public brls::Box {
public:
    explicit RomMPairingView(std::function<void(bool)> onFinished)
        : onFinished_(std::move(onFinished)) {
        auto& theme = ThemeManager::instance();
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setBackgroundColor(theme.color("brls/background"));
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setFocusable(true);
        this->setHideHighlight(true);

        titleLabel_ = new brls::Label();
        titleLabel_->setFontSize(32);
        titleLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        titleLabel_->setTextColor(theme.color("brls/text"));
        titleLabel_->setText("Pair RomM Device");
        this->addView(titleLabel_);

        hintLabel_ = new brls::Label();
        hintLabel_->setFontSize(20);
        hintLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        hintLabel_->setTextColor(theme.color("nxstation/detail_text"));
        hintLabel_->setMarginTop(16);
        hintLabel_->setSingleLine(false);
        hintLabel_->setText("Enter the 8-digit pairing code from RomM:\nSettings → Devices → Add Device");
        this->addView(hintLabel_);

        codeInput_ = new brls::InputCell();
        codeInput_->init("Pairing Code", "", [this](std::string text) {
            pairingCode_ = text;
        }, "12345678", "8-digit code from RomM", 8);
        codeInput_->setWidth(300);
        codeInput_->setMarginTop(24);
        this->addView(codeInput_);

        statusLabel_ = new brls::Label();
        statusLabel_->setFontSize(20);
        statusLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        statusLabel_->setTextColor(theme.color("nxstation/detail_text"));
        statusLabel_->setMarginTop(16);
        statusLabel_->setText("");
        this->addView(statusLabel_);

        pairButton_ = new brls::Button();
        pairButton_->setText("Pair Device");
        pairButton_->setWidth(300);
        pairButton_->setMarginTop(24);
        pairButton_->registerClickAction([this](brls::View*) { startPairing(); return true; });
        this->addView(pairButton_);

        this->registerAction("Cancel", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
            if (onFinished_) onFinished_(false);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
    }

    void startPairing() {
        if (pairingCode_.size() != 8) {
            statusLabel_->setText("Please enter an 8-digit code");
            return;
        }

        pairing_ = true;
        pairButton_->setState(brls::ButtonState::DISABLED);
        statusLabel_->setText("Pairing…");
        statusLabel_->setTextColor(ThemeManager::instance().color("brls/text"));

        AppState::instance().pool().enqueue([this, code = pairingCode_]() {
            auto& romm = sf::romm::RomMService::instance();
            std::string error;
            bool ok = romm.pairDevice(code, error);

            brls::sync([this, ok, error]() {
                pairing_ = false;
                pairButton_->setState(brls::ButtonState::ENABLED);

                if (ok) {
                    statusLabel_->setText("Device paired successfully!");
                    statusLabel_->setTextColor(ThemeManager::instance().color("success"));
                    brls::Application::notify("RomM device paired");
                    if (onFinished_) onFinished_(true);
                    brls::Application::popActivity(brls::TransitionAnimation::FADE);
                } else {
                    statusLabel_->setText("Pairing failed: " + error);
                    statusLabel_->setTextColor(ThemeManager::instance().color("error"));
                }
            });
        });
    }

private:
    std::function<void(bool)> onFinished_;
    brls::Label* titleLabel_ = nullptr;
    brls::Label* hintLabel_ = nullptr;
    brls::InputCell* codeInput_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::Button* pairButton_ = nullptr;
    std::string pairingCode_;
    bool pairing_ = false;
};

class RomMPairingActivity : public brls::Activity {
public:
    explicit RomMPairingActivity(std::function<void(bool)> onFinished)
        : onFinished_(std::move(onFinished)) {}

    brls::View* createContentView() override {
        return new RomMPairingView(onFinished_);
    }

private:
    std::function<void(bool)> onFinished_;
};

class RomMSyncView : public brls::Box {
public:
    RomMSyncView(std::function<void(bool)> onFinished)
        : onFinished_(std::move(onFinished)) {
        auto& theme = ThemeManager::instance();
        this->setAxis(brls::Axis::COLUMN);
        this->setGrow(1.0f);
        this->setBackgroundColor(theme.color("brls/background"));
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setJustifyContent(brls::JustifyContent::CENTER);
        this->setFocusable(true);
        this->setHideHighlight(true);

        titleLabel_ = new brls::Label();
        titleLabel_->setFontSize(32);
        titleLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        titleLabel_->setTextColor(theme.color("brls/text"));
        titleLabel_->setText("RomM Sync");
        this->addView(titleLabel_);

        // Sync runs over the ROMs that have a mapping (i.e. the ones downloaded from RomM).
        auto& romm = sf::romm::RomMService::instance();
        const size_t mapped = romm.mapper().allMappings().size();
        auto* countLabel = new brls::Label();
        countLabel->setFontSize(20);
        countLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        countLabel->setTextColor(theme.color("nxstation/detail_text"));
        countLabel->setMarginTop(8);
        countLabel->setText(mapped == 1 ? "1 ROM to sync" : std::to_string(mapped) + " ROMs to sync");
        this->addView(countLabel);

        progressTrack_ = new brls::Box();
        progressTrack_->setWidth(kTrackWidth);
        progressTrack_->setHeight(8);
        progressTrack_->setMarginTop(24);
        progressTrack_->setBackgroundColor(theme.color("nxstation/card_bg"));
        this->addView(progressTrack_);

        progressFill_ = new brls::Box();
        progressFill_->setHeight(8);
        progressFill_->setWidth(0);
        progressFill_->setBackgroundColor(theme.color("brls/accent"));
        this->addView(progressFill_);

        statusLabel_ = new brls::Label();
        statusLabel_->setFontSize(20);
        statusLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        statusLabel_->setTextColor(theme.color("nxstation/detail_text"));
        statusLabel_->setMarginTop(16);
        statusLabel_->setSingleLine(false);
        statusLabel_->setText("Preparing sync…");
        this->addView(statusLabel_);

        spinner_ = new brls::ProgressSpinner();
        spinner_->setMarginTop(16);
        this->addView(spinner_);

        this->registerAction("Cancel", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
            cancelled_ = true;
            if (onFinished_) onFinished_(false);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
    }

    void willAppear(bool resetState) override {
        *alive_ = true;
        brls::Box::willAppear(resetState);
        if (!running_) startSync();
    }

    void willDisappear(bool resetState) override {
        *alive_ = false;
        brls::Box::willDisappear(resetState);
    }

    void setStatus(const std::string& status) {
        if (!*alive_)
            return;
        statusLabel_->setText(status);
    }

    void setPercent(int percent) {
        if (!*alive_)
            return;
        percent_ = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
        progressFill_->setWidth(kTrackWidth * static_cast<float>(percent_) / 100.0f);
    }

private:
    void startSync() {
        running_ = true;
        auto& romm = sf::romm::RomMService::instance();
        auto alive = alive_;

        romm.syncAll([this, alive](bool success, const std::string& message) {
            if (!*alive)
                return;
            running_ = false;
            setStatus(message.empty() ? "Nothing to sync" : message);
            statusLabel_->setTextColor(ThemeManager::instance().color(success ? "success" : "error"));
            spinner_->setVisibility(brls::Visibility::GONE);

            if (onFinished_) onFinished_(success);
        }, [this, alive](const std::string& message, int percent) {
            if (!*alive)
                return;
            setStatus(message);
            setPercent(percent);
        });
    }

    static constexpr float kTrackWidth = 320.f;

    std::function<void(bool)> onFinished_;
    brls::Label* titleLabel_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::ProgressSpinner* spinner_ = nullptr;
    brls::Box* progressTrack_ = nullptr;
    brls::Box* progressFill_ = nullptr;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    int percent_ = 0;
    bool running_ = false;
    bool cancelled_ = false;
};

void buildRomMSettingsRows(brls::Box* listBox) {
    auto& config = sf::Config::instance();
    auto& rommConfig = config.rommConfig();
    auto& romm = sf::romm::RomMService::instance();

    // Enable toggle
    auto* enableRow = makeSettingsRow("Enable RomM Sync", rommConfig.enabled ? "On" : "Off");
    enableRow->registerClickAction([enableRow, &rommConfig, &config](brls::View*) {
        rommConfig.enabled = !rommConfig.enabled;
        enableRow->setDetailText(rommConfig.enabled ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(enableRow);
    listBox->addView(enableRow);

    // Server URL
    auto* serverRow = makeSettingsRow("Server URL", rommConfig.serverUrl.empty() ? "Not set" : rommConfig.serverUrl);
    serverRow->registerClickAction([serverRow, &rommConfig, &config](brls::View*) {
        auto* kb = new brls::InputCell();
        kb->init("RomM Server URL", rommConfig.serverUrl, [serverRow, &rommConfig, &config](std::string text) {
            rommConfig.serverUrl = text;
            serverRow->setDetailText(text.empty() ? "Not set" : text);
            config.saveUserSettings();
            config.saveRomMConfig();
        }, rommConfig.serverUrl, "https://romm.example.com");
        auto* dialog = new brls::Dialog(kb);
        dialog->addButton("OK", [kb, serverRow, &rommConfig, &config, dialog]() {
            rommConfig.serverUrl = kb->getValue();
            serverRow->setDetailText(kb->getValue().empty() ? "Not set" : kb->getValue());
            config.saveUserSettings();
            config.saveRomMConfig();
            dialog->close();
        });
        dialog->addButton("Cancel", [dialog]() { dialog->close(); });
        dialog->open();
        return true;
    });
    styleSettingsCell(serverRow);
    listBox->addView(serverRow);

    // Device Name
    auto* deviceNameRow = makeSettingsRow("Device Name", rommConfig.deviceName.empty() ? "NXStation" : rommConfig.deviceName);
    deviceNameRow->registerClickAction([deviceNameRow, &rommConfig, &config](brls::View*) {
        auto* kb = new brls::InputCell();
        kb->init("Device Name", rommConfig.deviceName, [deviceNameRow, &rommConfig, &config](std::string text) {
            rommConfig.deviceName = text;
            deviceNameRow->setDetailText(text.empty() ? "NXStation" : text);
            config.saveUserSettings();
            config.saveRomMConfig();
        }, rommConfig.deviceName, "NXStation");
        auto* dialog = new brls::Dialog(kb);
        dialog->addButton("OK", [kb, deviceNameRow, &rommConfig, &config, dialog]() {
            rommConfig.deviceName = kb->getValue();
            deviceNameRow->setDetailText(kb->getValue().empty() ? "NXStation" : kb->getValue());
            config.saveUserSettings();
            config.saveRomMConfig();
            dialog->close();
        });
        dialog->addButton("Cancel", [dialog]() { dialog->close(); });
        dialog->open();
        return true;
    });
    styleSettingsCell(deviceNameRow);
    listBox->addView(deviceNameRow);

    // Pair/Unpair button
    if (!romm.isPaired()) {
        auto* pairRow = makeSettingsRow("Pair Device", "Enter 8-digit code from RomM");
        pairRow->registerClickAction([](brls::View*) {
            auto* view = new RomMPairingView([](bool) {});
            brls::Application::pushActivity(new RomMPairingActivity([](bool) {}));
            return true;
        });
        styleSettingsCell(pairRow);
        listBox->addView(pairRow);
    } else {
        auto* deviceRow = makeSettingsRow("Paired Device", romm.deviceId());
        styleSettingsCell(deviceRow);
        listBox->addView(deviceRow);

        auto* unpairRow = makeSettingsRow("Unpair Device", "Disconnect from RomM");
        unpairRow->registerClickAction([&romm, &rommConfig, &config](brls::View*) {
            auto* label = new brls::Label();
            label->setText("Disconnect from RomM?\nThis will remove the device pairing.");
            label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            label->setSingleLine(false);
            label->setLineHeight(1.35f);
            label->setMargins(24, 24, 24, 24);

            auto* box = new brls::Box();
            box->setAxis(brls::Axis::COLUMN);
            box->addView(label);

            brls::Dialog* dialog = new brls::Dialog(box);
            dialog->addButton("Cancel", [] {});
            dialog->addButton("Disconnect", [&romm, &rommConfig, &config]() {
                std::string error;
                romm.disconnect(error);
                config.saveUserSettings();
                config.saveRomMConfig();
                brls::Application::notify("RomM device disconnected");
            });
            dialog->open();
            return true;
        });
        styleSettingsCell(unpairRow);
        listBox->addView(unpairRow);
    }

    // Sync options
    auto* syncSavesRow = makeSettingsRow("Sync Save Files", rommConfig.syncSaves ? "On" : "Off");
    syncSavesRow->registerClickAction([syncSavesRow, &rommConfig, &config](brls::View*) {
        rommConfig.syncSaves = !rommConfig.syncSaves;
        syncSavesRow->setDetailText(rommConfig.syncSaves ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(syncSavesRow);
    listBox->addView(syncSavesRow);

    auto* syncStatesRow = makeSettingsRow("Sync Save States", rommConfig.syncStates ? "On" : "Off");
    syncStatesRow->registerClickAction([syncStatesRow, &rommConfig, &config](brls::View*) {
        rommConfig.syncStates = !rommConfig.syncStates;
        syncStatesRow->setDetailText(rommConfig.syncStates ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(syncStatesRow);
    listBox->addView(syncStatesRow);

    auto* syncBeforeLaunchRow = makeSettingsRow("Sync Before Launch", rommConfig.syncBeforeLaunch ? "On" : "Off");
    syncBeforeLaunchRow->registerClickAction([syncBeforeLaunchRow, &rommConfig, &config](brls::View*) {
        rommConfig.syncBeforeLaunch = !rommConfig.syncBeforeLaunch;
        syncBeforeLaunchRow->setDetailText(rommConfig.syncBeforeLaunch ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(syncBeforeLaunchRow);
    listBox->addView(syncBeforeLaunchRow);

    auto* syncAfterReturnRow = makeSettingsRow("Sync After Return", rommConfig.syncAfterReturn ? "On" : "Off");
    syncAfterReturnRow->registerClickAction([syncAfterReturnRow, &rommConfig, &config](brls::View*) {
        rommConfig.syncAfterReturn = !rommConfig.syncAfterReturn;
        syncAfterReturnRow->setDetailText(rommConfig.syncAfterReturn ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(syncAfterReturnRow);
    listBox->addView(syncAfterReturnRow);

    // Conflict policy
    auto* conflictRow = makeSettingsRow("Conflict Policy", sf::romm::conflictPolicyToString(rommConfig.conflictPolicy));
    conflictRow->registerClickAction([conflictRow, &rommConfig, &config](brls::View*) {
        using Policy = sf::romm::ConflictPolicy;
        Policy next;
        switch (rommConfig.conflictPolicy) {
            case Policy::KeepBoth: next = Policy::KeepLocal; break;
            case Policy::KeepLocal: next = Policy::KeepRemote; break;
            case Policy::KeepRemote: next = Policy::Ask; break;
            case Policy::Ask: next = Policy::KeepBoth; break;
        }
        rommConfig.conflictPolicy = next;
        conflictRow->setDetailText(sf::romm::conflictPolicyToString(next));
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(conflictRow);
    listBox->addView(conflictRow);

    // State core matching
    auto* coreMatchRow = makeSettingsRow("Core Matching for States", rommConfig.stateCoreMatching ? "On" : "Off");
    coreMatchRow->registerClickAction([coreMatchRow, &rommConfig, &config](brls::View*) {
        rommConfig.stateCoreMatching = !rommConfig.stateCoreMatching;
        coreMatchRow->setDetailText(rommConfig.stateCoreMatching ? "On" : "Off");
        config.saveUserSettings();
        config.saveRomMConfig();
        return true;
    });
    styleSettingsCell(coreMatchRow);
    listBox->addView(coreMatchRow);

    // Manual sync button
    // Last sync status (declared before Sync Now so the sync callback can update it)
    auto* lastSyncRow = makeSettingsRow("Last Sync", romm.lastSyncResult().empty() ? "Never" : romm.lastSyncResult());

    auto* syncNowRow = makeSettingsRow("Sync Now", "Sync every ROM downloaded from RomM");
    syncNowRow->registerClickAction([lastSyncRow, &romm](brls::View*) {
        auto* view = new RomMSyncView([lastSyncRow, &romm](bool) {
            lastSyncRow->setDetailText(romm.lastSyncResult().empty() ? "Never" : romm.lastSyncResult());
        });
        brls::Application::pushActivity(new RomMSyncActivity(view));
        return true;
    });
    styleSettingsCell(syncNowRow);
    listBox->addView(syncNowRow);

    // Library refresh (populates the RomM virtual library on the main page)
    auto* refreshRow = makeSettingsRow("Refresh Library", "Fetch the RomM library from the server");
    refreshRow->registerClickAction([refreshRow, &romm](brls::View*) {
        refreshRow->setDetailText("Refreshing…");
        romm.refreshLibraryAsync([refreshRow, &romm](bool ok, const std::string& message) {
            if (!ok) {
                refreshRow->setDetailText(message.empty() ? "Refresh failed" : message);
                brls::Application::notify("RomM library refresh failed: " + message);
                return;
            }
            romm.populateVirtualLibrary();
            sf::ui::SystemsTab::requestRefreshAfterSettings();
            refreshRow->setDetailText(std::to_string(romm.library().entries().size()) + " ROMs cached");
            brls::Application::notify("RomM library refreshed");
        });
        return true;
    });
    styleSettingsCell(refreshRow);
    listBox->addView(refreshRow);

    // Last sync status
    styleSettingsCell(lastSyncRow);
    listBox->addView(lastSyncRow);

    applyThemeToSettingsPanel(nullptr, listBox);
}

} // namespace sf::ui
