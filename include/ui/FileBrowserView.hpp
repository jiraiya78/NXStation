#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace sf::ui {

/** Simple SD-card file browser for selecting .nro cores and similar paths. */
class FileBrowserView : public brls::Box {
public:
    using SelectCallback = std::function<void(std::string path)>;

    FileBrowserView(std::string startDir, std::vector<std::string> extensions, SelectCallback onSelect,
                    std::string title = "Select File", bool selectDirectories = false);

    static void present(std::string startDir, std::vector<std::string> extensions, SelectCallback onSelect,
                        std::string title = "Select File");

    /** Browse for a folder instead of a file; adds a "Select This Folder" row. */
    static void presentForDirectory(std::string startDir, SelectCallback onSelect,
                                    std::string title = "Select Folder");

private:
    void navigateTo(const std::string& path);
    void rebuildList();
    bool matchesFilter(const std::string& path, bool isDir) const;
    std::string shortPath(const std::string& path) const;

    std::string currentDir_;
    std::vector<std::string> extensions_;
    SelectCallback onSelect_;
    std::string title_;
    bool selectDirectories_ = false;

    brls::Header* header_ = nullptr;

    BRLS_BIND(brls::ScrollingFrame, scroller, "settings/scroller");
    BRLS_BIND(brls::Box, listBox, "settings/list");
};

} // namespace sf::ui
