#pragma once

#include <cstddef>
#include <string>

namespace sf {

struct SavedNavigation {
    bool shouldRestore = false;
    std::string systemId;
    size_t gameIndex = 0;
    std::string romPath;
};

/** Persists last-opened system list + game focus across NRO handoff. */
class NavigationState {
public:
    static void update(const std::string& systemId, size_t gameIndex, const std::string& romPath);
    static void persistForHandoff();
    static SavedNavigation takePendingRestore();

    /** Remember which carousel entry the user was on, so the next boot starts there. */
    static void setLastSystem(const std::string& systemId);
    static std::string lastSystem();
};

} // namespace sf
