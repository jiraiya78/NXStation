#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace sf::ui {

class FocusedMenuDialog;

/** Pick a system from the main carousel (Y menu → Jump to System). */
class SystemJumpView : public brls::Box {
public:
    using PickCallback = std::function<void(std::string systemId)>;

    SystemJumpView(std::vector<std::string> systemIds, PickCallback onPick);
    ~SystemJumpView() override = default;

    static void present(std::vector<std::string> systemIds, PickCallback onPick,
                        std::function<void()> onDismiss = nullptr);

private:
    FocusedMenuDialog* dialog_ = nullptr;
    PickCallback onPick_;
};

} // namespace sf::ui
