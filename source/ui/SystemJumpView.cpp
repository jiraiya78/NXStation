#include "ui/SystemJumpView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "app/Config.hpp"
#include "util/ActionLog.hpp"
#include "util/VirtualSystems.hpp"

#include <borealis/views/cells/cell_detail.hpp>

namespace sf::ui {

SystemJumpView::SystemJumpView(std::vector<std::string> systemIds, PickCallback onPick)
    : onPick_(std::move(onPick))
{
    stylePopupMenuPanel(this);

    auto* header = new brls::Header();
    header->setTitle("Jump to System");
    header->setSubtitle("Pick a library");
    this->addView(header);

    auto menuScroller = makePopupMenuScroller(420.f);
    brls::ScrollingFrame* scroller = menuScroller.scroller;

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setClipsToBounds(true);

    for (const std::string& id : systemIds) {
        std::string label;
        if (isVirtualSystemId(id))
            label = virtualSystemDisplayName(id);
        else if (const SystemConfig* sys = Config::instance().findSystem(id))
            label = sys->name;
        else
            label = id;

        auto* row = new brls::DetailCell();
        row->setText(label);
        stylePopupMenuRow(row);
        row->registerClickAction([this, id](brls::View*) {
            SF_LOG_ACTION("Systems/JumpToSystem");
            auto callback = onPick_;
            if (dialog_) {
                dialog_->close([callback, id] {
                    if (callback)
                        callback(id);
                });
            } else if (callback) {
                callback(id);
            }
            return true;
        });
        list->addView(row);
    }

    scroller->setContentView(list);
    this->addView(menuScroller.clipBox);
    registerPopupPageActions(this, scroller);
}

void SystemJumpView::present(std::vector<std::string> systemIds, PickCallback onPick,
                             std::function<void()> onDismiss)
{
    auto* menu = new SystemJumpView(std::move(systemIds), std::move(onPick));
    auto* dialog = FocusedMenuDialog::present(menu, std::move(onDismiss));
    menu->dialog_ = dialog;
}

} // namespace sf::ui
