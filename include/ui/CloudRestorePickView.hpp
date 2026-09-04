#pragma once

#include "cloud/CloudSaveService.hpp"

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sf::ui {

class CloudRestorePickView : public brls::Box {
public:
    CloudRestorePickView();
    ~CloudRestorePickView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;

    static void present();

private:
    void showLoading();
    void showError(const std::string& message);
    void showBackups(const std::vector<sf::cloud::CloudBackupInfo>& backups);
    void confirmRestore(const sf::cloud::CloudBackupInfo& backup);
    void loadBackups();
    void focusBackupList();

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    bool loading_ = false;

    brls::Label* warningLabel_ = nullptr;
    brls::Label* pathsLabel_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::ScrollingFrame* listScroller_ = nullptr;
    brls::Box* listBox_ = nullptr;
};

} // namespace sf::ui
