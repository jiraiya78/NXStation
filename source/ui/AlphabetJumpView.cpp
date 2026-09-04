#include "ui/AlphabetJumpView.hpp"
#include "ui/FocusedMenuDialog.hpp"
#include "ui/ThemeManager.hpp"
#include "util/ActionLog.hpp"

#include <string>
#include <vector>

namespace sf::ui {

namespace {

constexpr int kColumns = 10;
constexpr int kTileSize = 56;
constexpr int kTileGap = 6;

/** Rows are laid out by hand: digits first, then the alphabet, then "other". */
std::vector<std::string> buildRows()
{
    std::vector<std::string> rows{"0123456789"};

    std::string letters;
    for (char c = 'A'; c <= 'Z'; ++c)
        letters.push_back(c);
    letters.push_back('#');

    for (size_t i = 0; i < letters.size(); i += kColumns)
        rows.push_back(letters.substr(i, kColumns));

    return rows;
}

} // namespace

AlphabetJumpView::AlphabetJumpView(PickCallback onPick)
    : onPick_(std::move(onPick))
{
    auto& theme = ThemeManager::instance();
    stylePopupMenuPanel(this);

    auto* header = new brls::Header();
    header->setTitle("Jump to");
    header->setSubtitle("Pick a letter or number");
    this->addView(header);

    auto* grid = new brls::Box();
    grid->setAxis(brls::Axis::COLUMN);
    grid->setMarginTop(12);

    for (const std::string& rowText : buildRows()) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setJustifyContent(brls::JustifyContent::CENTER);
        row->setMarginBottom(kTileGap);
        grid->addView(row);

        for (size_t i = 0; i < rowText.size(); ++i) {
            const char bucket = rowText[i];

            auto* tile = new brls::Box();
            tile->setAxis(brls::Axis::COLUMN);
            tile->setWidth(kTileSize);
            tile->setHeight(kTileSize);
            tile->setAlignItems(brls::AlignItems::CENTER);
            tile->setJustifyContent(brls::JustifyContent::CENTER);
            tile->setCornerRadius(8);
            tile->setBackgroundColor(theme.color("nxstation/dialog_row"));
            tile->setFocusable(true);
            if (i + 1 < rowText.size())
                tile->setMarginRight(kTileGap);

            auto* label = new brls::Label();
            label->setText(std::string(1, bucket));
            label->setFontSize(26);
            label->setTextColor(theme.color("nxstation/title_text"));
            tile->addView(label);

            tile->registerClickAction([this, bucket](brls::View*) {
                SF_LOG_ACTION("GameList/AlphabetJump");
                auto callback = onPick_;
                if (dialog_) {
                    dialog_->close([callback, bucket] {
                        if (callback)
                            callback(bucket);
                    });
                } else if (callback) {
                    callback(bucket);
                }
                return true;
            });

            row->addView(tile);
        }
    }

    this->addView(grid);
}

void AlphabetJumpView::present(PickCallback onPick, std::function<void()> onDismiss)
{
    auto* menu = new AlphabetJumpView(std::move(onPick));
    auto* dialog = FocusedMenuDialog::present(menu, std::move(onDismiss));
    menu->dialog_ = dialog;
}

} // namespace sf::ui
