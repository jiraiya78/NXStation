#pragma once

#include <borealis.hpp>
#include <functional>

namespace sf::ui {

/** Grid picker for A–Z, 0–9, and # buckets. */
class AlphabetJumpView : public brls::Box {
public:
    using PickCallback = std::function<void(char bucket)>;

    AlphabetJumpView(PickCallback onPick);
    ~AlphabetJumpView() override = default;

    static void present(PickCallback onPick, std::function<void()> onDismiss = nullptr);

private:
    class FocusedMenuDialog* dialog_ = nullptr;
    PickCallback onPick_;
};

} // namespace sf::ui
