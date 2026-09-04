#pragma once

#include <string>

namespace sf::analytics {

/** Track play sessions for NXStation core-direct launches (RetroArch .lrtl is not written). */
class PlaySessionTracker {
public:
    static void beginSession(const std::string& systemId, const std::string& romPath,
                             const std::string& romName = {}, const std::string& coreName = {});

    /** Finalize a pending session after returning from a game (no-op if none). */
    static void commitPendingSession();

private:
    static std::string normalizeKey(const std::string& romPath);
};

} // namespace sf::analytics
