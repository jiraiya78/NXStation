#include "ui/CloudRestorePickView.hpp"
#include "cloud/RetroArchPaths.hpp"
#include "ui/CloudRestoreProgressView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/ActionLog.hpp"

#include <sstream>

namespace sf::ui {

namespace {

std::string formatSize(int64_t bytes)
{
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024)
        return std::to_string(bytes / 1024) + " KB";
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    return out.str();
}

brls::DetailCell* makeBackupRow(const sf::cloud::CloudBackupInfo& backup)
{
    auto& theme = ThemeManager::instance();
    auto* row = new brls::DetailCell();
    row->setText(backup.displayLabel);
    row->setDetailText(formatSize(backup.size));
    row->setTextColor(theme.color("nxstation/title_text"));
    row->setDetailTextColor(theme.color("nxstation/detail_text"));
    row->setHeight(58);
    row->setFocusable(true);
    return row;
}

} // namespace

CloudRestorePickView::CloudRestorePickView()
{
    auto& theme = ThemeManager::instance();
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.f);
    this->setBackgroundColor(theme.color("nxstation/overlay_bg"));
    this->setPadding(28, 36, 36, 36);
    this->setFocusable(false);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    auto* title = new brls::Label();
    title->setText("Cloud Restore");
    title->setFontSize(30);
    title->setTextColor(theme.color("brls/text"));
    title->setMarginBottom(12);
    this->addView(title);

    warningLabel_ = new brls::Label();
    warningLabel_->setFontSize(18);
    warningLabel_->setSingleLine(false);
    warningLabel_->setLineHeight(1.35f);
    warningLabel_->setTextColor(theme.color("nxstation/log_failure"));
    warningLabel_->setText(
        "NXStation backup only. These archives were created by NXStation cloud backup and use "
        "NXStation's folder layout. RetroArch's own cloud backup (if any) uses a different "
        "format — do not use them interchangeably.");
    warningLabel_->setMarginBottom(14);
    this->addView(warningLabel_);

    const sf::cloud::RetroArchSaveRoots roots = sf::cloud::discoverRetroArchSaveRoots();
    pathsLabel_ = new brls::Label();
    pathsLabel_->setFontSize(17);
    pathsLabel_->setSingleLine(false);
    pathsLabel_->setLineHeight(1.3f);
    pathsLabel_->setTextColor(theme.color("nxstation/detail_text"));
    pathsLabel_->setText("Restore targets (from retroarch.cfg):\nSaves: " + roots.savesDir
                         + "\nStates: " + roots.statesDir);
    pathsLabel_->setMarginBottom(16);
    this->addView(pathsLabel_);

    statusLabel_ = new brls::Label();
    statusLabel_->setFontSize(20);
    statusLabel_->setTextColor(theme.color("nxstation/log_progress"));
    statusLabel_->setText("Loading backups…");
    statusLabel_->setMarginBottom(12);
    this->addView(statusLabel_);

    listScroller_ = new brls::ScrollingFrame();
    listScroller_->setGrow(1.f);
    listScroller_->setScrollingIndicatorVisible(false);
    listScroller_->setClipsToBounds(true);

    listBox_ = new brls::Box();
    listBox_->setAxis(brls::Axis::COLUMN);
    listBox_->setGrow(1.f);
    listScroller_->setContentView(listBox_);
    this->addView(listScroller_);

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            playNavSfx();
            const float page = listScroller_->getHeight() * 0.85f;
            listScroller_->setContentOffsetY(std::max(0.f, listScroller_->getContentOffsetY() - page), true);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            playNavSfx();
            const float page = listScroller_->getHeight() * 0.85f;
            listScroller_->setContentOffsetY(listScroller_->getContentOffsetY() + page, true);
            return true;
        });
}

CloudRestorePickView::~CloudRestorePickView()
{
    *alive_ = false;
}

void CloudRestorePickView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    focusBackupList();
    if (!loading_) {
        loading_ = true;
        loadBackups();
    }
}

void CloudRestorePickView::focusBackupList()
{
    if (!listBox_)
        return;
    const auto& children = listBox_->getChildren();
    if (children.empty())
        return;
    brls::Application::giveFocus(children.front());
}

void CloudRestorePickView::willDisappear(bool resetState)
{
    *alive_ = false;
    brls::Box::willDisappear(resetState);
}

void CloudRestorePickView::showLoading()
{
    statusLabel_->setText("Loading backups…");
    listBox_->clearViews();
}

void CloudRestorePickView::showError(const std::string& message)
{
    loading_ = false;
    statusLabel_->setText(message);
    statusLabel_->setTextColor(ThemeManager::instance().color("nxstation/log_failure"));
    listBox_->clearViews();
}

void CloudRestorePickView::showBackups(const std::vector<sf::cloud::CloudBackupInfo>& backups)
{
    loading_ = false;
    listBox_->clearViews();

    if (backups.empty()) {
        statusLabel_->setText("No NXStation backups found on Google Drive");
        statusLabel_->setTextColor(ThemeManager::instance().color("nxstation/muted_text"));
        return;
    }

    statusLabel_->setText("Select a backup to restore (merge into current folders)");
    statusLabel_->setTextColor(ThemeManager::instance().color("nxstation/log_progress"));

    for (const auto& backup : backups) {
        auto* row = makeBackupRow(backup);
        row->registerClickAction([this, backup](brls::View*) {
            SF_LOG_ACTION("CloudRestore/Pick");
            playToggleSfx();
            confirmRestore(backup);
            return true;
        });
        listBox_->addView(row);
    }

    listScroller_->setContentOffsetY(0.f, false);
    focusBackupList();
}

void CloudRestorePickView::confirmRestore(const sf::cloud::CloudBackupInfo& backup)
{
    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setPadding(24, 28, 28, 28);

    auto* label = new brls::Label();
    label->setSingleLine(false);
    label->setLineHeight(1.35f);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setText(
        "Restore \"" + backup.displayLabel + "\"?\n\n"
        "Files from this NXStation backup will be merged into your current RetroArch save and "
        "state folders (and ROM sidecars where applicable). Existing local files are kept unless "
        "the backup contains a file with the same path.\n\n"
        "A local pre-restore ZIP will be created first.\n\n"
        "This is not a RetroArch cloud backup — do not mix backup types.");
    panel->addView(label);

    auto* dialog = new brls::Dialog(panel);
    dialog->setCancelable(true);
    dialog->addButton(
        "Restore",
        [backup]() { CloudRestoreProgressView::present(backup.id, backup.name); });
    dialog->addButton("Cancel", [] {});
    dialog->open();
}

void CloudRestorePickView::loadBackups()
{
    showLoading();
    auto alive = alive_;
    sf::cloud::CloudSaveService::instance().listBackups(
        [this, alive](std::vector<sf::cloud::CloudBackupInfo> backups, std::string error) {
            if (!*alive)
                return;
            if (!error.empty())
                showError(error);
            else
                showBackups(backups);
        });
}

void CloudRestorePickView::present()
{
    PushedActivity::push(new CloudRestorePickView());
}

} // namespace sf::ui
