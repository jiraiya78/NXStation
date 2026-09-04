#include "ui/FileBrowserView.hpp"
#include "ui/PushedActivity.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/ActionLog.hpp"
#include "util/FileSystem.hpp"

#include <algorithm>
#include <borealis/views/cells/cell_detail.hpp>

namespace sf::ui {

namespace {

brls::DetailCell* makeRow(const std::string& title, const std::string& detail = {})
{
    auto* row = new brls::DetailCell();
    row->setText(title);
    if (!detail.empty())
        row->setDetailText(detail);
    row->setTextColor(ThemeManager::instance().color("nxstation/title_text"));
    row->setDetailTextColor(ThemeManager::instance().color("nxstation/detail_text"));
    return row;
}

std::string pickStartDir(const std::string& hint)
{
    if (!hint.empty()) {
        const std::string parent = FileSystem::parentPath(hint);
        if (!parent.empty() && FileSystem::isDirectory(parent))
            return parent;
    }

    const char* candidates[] = {
        "sdmc:/retroarch/cores",
        "sdmc:/switch/retroarch/cores",
        "sdmc:/retroarch",
        "sdmc:/switch",
        "sdmc:/",
    };
    for (const char* c : candidates) {
        if (FileSystem::isDirectory(c))
            return c;
    }
    return "sdmc:/";
}

} // namespace

FileBrowserView::FileBrowserView(std::string startDir, std::vector<std::string> extensions,
                                 SelectCallback onSelect, std::string title, bool selectDirectories)
    : currentDir_(std::move(startDir))
    , extensions_(std::move(extensions))
    , onSelect_(std::move(onSelect))
    , title_(std::move(title))
    , selectDirectories_(selectDirectories)
{
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    this->setBackgroundColor(ThemeManager::instance().color("brls/background"));
    scroller->setScrollingIndicatorVisible(false);

    if (!FileSystem::isDirectory(currentDir_))
        currentDir_ = pickStartDir({});

    rebuildList();

    this->registerAction(
        "Back", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
            const std::string parent = FileSystem::parentPath(currentDir_);
            if (!parent.empty() && parent != currentDir_) {
                playNavSfx();
                navigateTo(parent);
                return true;
            }
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });

    this->registerAction(
        "Page Up", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(std::max(0.f, scroller->getContentOffsetY() - page), true);
            return true;
        });
    this->registerAction(
        "Page Down", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
            playNavSfx();
            const float page = scroller->getHeight() * 0.85f;
            scroller->setContentOffsetY(scroller->getContentOffsetY() + page, true);
            return true;
        });
}

void FileBrowserView::present(std::string startDir, std::vector<std::string> extensions,
                              SelectCallback onSelect, std::string title)
{
    if (!FileSystem::isDirectory(startDir))
        startDir = pickStartDir(startDir);
    playConfirmSfx();
    PushedActivity::push(new FileBrowserView(std::move(startDir), std::move(extensions),
                                             std::move(onSelect), std::move(title)));
}

void FileBrowserView::presentForDirectory(std::string startDir, SelectCallback onSelect, std::string title)
{
    if (!FileSystem::isDirectory(startDir))
        startDir = pickStartDir(startDir);
    playConfirmSfx();
    PushedActivity::push(new FileBrowserView(std::move(startDir), {}, std::move(onSelect),
                                             std::move(title), true));
}

bool FileBrowserView::matchesFilter(const std::string& path, bool isDir) const
{
    if (isDir)
        return true;
    if (selectDirectories_)
        return false;
    if (extensions_.empty())
        return true;
    const std::string ext = FileSystem::extensionOf(path);
    for (const auto& e : extensions_) {
        if (FileSystem::toLower(e) == ext)
            return true;
    }
    return false;
}

std::string FileBrowserView::shortPath(const std::string& path) const
{
    if (path.size() <= 52)
        return path;
    return "…" + path.substr(path.size() - 50);
}

void FileBrowserView::navigateTo(const std::string& path)
{
    if (!FileSystem::isDirectory(path))
        return;
    currentDir_ = path;
    rebuildList();
    scroller->setContentOffsetY(0.f, false);
}

void FileBrowserView::rebuildList()
{
    listBox->clearViews();

    header_ = new brls::Header();
    header_->setTitle(title_);
    header_->setSubtitle(shortPath(currentDir_));
    listBox->addView(header_);

    if (selectDirectories_) {
        auto* select = makeRow("Select This Folder", shortPath(currentDir_));
        select->registerClickAction([this](brls::View*) {
            SF_LOG_ACTION("FileBrowser/SelectFolder");
            playConfirmSfx();
            if (onSelect_)
                onSelect_(currentDir_);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
        listBox->addView(select);
    }

    const std::string parent = FileSystem::parentPath(currentDir_);
    if (!parent.empty() && parent != currentDir_) {
        auto* up = makeRow("..", "Parent folder");
        up->registerClickAction([this, parent](brls::View*) {
            SF_LOG_ACTION("FileBrowser/Up");
            playNavSfx();
            navigateTo(parent);
            return true;
        });
        listBox->addView(up);
    }

    auto entries = FileSystem::listDirectory(currentDir_);
    for (const auto& entry : entries) {
        if (!matchesFilter(entry.path, entry.isDirectory))
            continue;

        std::string detail;
        if (entry.isDirectory)
            detail = "Folder";
        else if (entry.size > 0)
            detail = std::to_string(entry.size / 1024) + " KB";

        auto* row = makeRow(entry.name, detail);
        const std::string path = entry.path;
        const bool isDir = entry.isDirectory;
        row->registerClickAction([this, path, isDir](brls::View*) {
            if (isDir) {
                SF_LOG_ACTION("FileBrowser/OpenDir");
                playNavSfx();
                navigateTo(path);
                return true;
            }
            SF_LOG_ACTION("FileBrowser/Select");
            playConfirmSfx();
            if (onSelect_)
                onSelect_(path);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
        listBox->addView(row);
    }

    const size_t reservedRows = 1 + (selectDirectories_ ? 1 : 0) + (parent.empty() || parent == currentDir_ ? 0 : 1);
    if (listBox->getChildren().size() <= reservedRows) {
        auto* empty = makeRow("(empty)", extensions_.empty() ? "No files here" : "No matching files");
        empty->setFocusable(false);
        listBox->addView(empty);
    }
}

} // namespace sf::ui
