#include "ui/ThemePickerView.hpp"

#include "ui/FocusedMenuDialog.hpp"
#include "ui/ThemeManager.hpp"
#include "ui/UiSfx.hpp"
#include "util/ActionLog.hpp"

#include <borealis.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/header.hpp>
#include <borealis/views/label.hpp>

namespace sf::ui {
namespace {

class ThemeListPanel : public brls::Box {
public:
    ThemeListPanel(const std::string& title, const std::string& subtitle,
                   const std::vector<std::string>& folders,
                   std::function<void(std::string)> onPick)
        : onPick_(std::move(onPick))
    {
        stylePopupMenuPanel(this);

        auto* header = new brls::Header();
        header->setTitle(title);
        if (!subtitle.empty())
            header->setSubtitle(subtitle);
        this->addView(header);

        auto menuScroller = makePopupMenuScroller(420.f);
        auto* list = new brls::Box();
        list->setAxis(brls::Axis::COLUMN);

        if (folders.empty()) {
            auto* empty = new brls::Label();
            empty->setText("No themes found in data/theme");
            empty->setFontSize(18);
            empty->setTextColor(ThemeManager::instance().color("nxstation/muted_text"));
            list->addView(empty);
        }

        const std::string current = ThemeManager::instance().currentTheme();
        for (const auto& folder : folders) {
            auto* row = new brls::DetailCell();
            std::string label = ThemeManager::instance().displayLabel(folder);
            if (folder == current)
                label += "  ✓";
            row->setText(label);
            stylePopupMenuRow(row);
            row->registerClickAction([this, folder](brls::View*) {
                playConfirmSfx();
                auto callback = onPick_;
                auto* dialog = dialog_;
                if (dialog) {
                    dialog->close([callback, folder] {
                        if (callback)
                            callback(folder);
                    });
                } else if (callback) {
                    callback(folder);
                }
                return true;
            });
            list->addView(row);
        }

        menuScroller.scroller->setContentView(list);
        this->addView(menuScroller.clipBox);
        registerPopupPageActions(this, menuScroller.scroller);
    }

    FocusedMenuDialog* dialog_ = nullptr;

private:
    std::function<void(std::string)> onPick_;
};

} // namespace

void ThemePickerView::present(Callback onSelect)
{
    SF_LOG_ACTION("Settings/ThemePicker");
    playConfirmSfx();

    const auto themes = ThemeManager::instance().availableThemes();
    auto* panel = new ThemeListPanel("Select Theme", "Themes live in sdmc:/switch/NXStation/data/theme/",
                                     themes, std::move(onSelect));
    auto* dialog = FocusedMenuDialog::present(panel);
    panel->dialog_ = dialog;
}

} // namespace sf::ui
