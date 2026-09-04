#pragma once

namespace sf {

/** Switch NIFM network session helpers for HTTP/scraping. */
class Network {
public:
    static bool waitForConnection(int timeoutSeconds = 30);
    static bool isAvailable();
    static bool acquireSession();
    static void releaseSession();
    /** Cancel NIFM session at app shutdown only. */
    static void shutdown();
};

} // namespace sf
