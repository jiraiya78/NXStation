#include "util/Network.hpp"
#include "util/Logger.hpp"

#include <chrono>
#include <thread>

#ifdef __SWITCH__
extern "C" {
#include <switch/services/nifm.h>
}
#endif

namespace sf {

namespace {

#ifdef __SWITCH__
NifmRequest g_request{};
bool g_requestOpen = false;
int g_sessionRefs = 0;
#endif

} // namespace

bool Network::acquireSession()
{
#ifdef __SWITCH__
    if (!g_requestOpen) {
        const Result rc = nifmCreateRequest(&g_request, true);
        if (R_FAILED(rc)) {
            SF_LOG_W("Net", "nifmCreateRequest failed (0x%x)", static_cast<unsigned>(rc));
            return false;
        }
        g_requestOpen = true;
    }

    const Result rc = nifmRequestSubmitAndWait(&g_request);
    if (R_FAILED(rc)) {
        SF_LOG_W("Net", "nifmRequestSubmitAndWait failed (0x%x)", static_cast<unsigned>(rc));
        return false;
    }

    g_sessionRefs++;
    SF_LOG_I("Net", "Network session acquired (refs=%d)", g_sessionRefs);
    return true;
#else
    return true;
#endif
}

void Network::releaseSession()
{
#ifdef __SWITCH__
    if (g_sessionRefs <= 0)
        return;
    g_sessionRefs--;
    SF_LOG_I("Net", "Network session released (refs=%d)", g_sessionRefs);
    // Do not call nifmRequestCancel here — canceling between HTTP requests breaks DNS
    // resolution on some Switch networks (CURLE_COULDNT_RESOLVE_HOST on re-acquire).
#endif
}

void Network::shutdown()
{
#ifdef __SWITCH__
    g_sessionRefs = 0;
    if (!g_requestOpen)
        return;
    nifmRequestCancel(&g_request);
    g_requestOpen = false;
    SF_LOG_I("Net", "Network session shutdown");
#endif
}

bool Network::waitForConnection(int timeoutSeconds)
{
#ifdef __SWITCH__
    if (isAvailable() && acquireSession())
        return true;

    SF_LOG_I("Net", "Waiting for network (up to %ds)…", timeoutSeconds);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    while (std::chrono::steady_clock::now() < deadline) {
        if (acquireSession())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    SF_LOG_W("Net", "Timed out waiting for network");
    return false;
#else
    (void)timeoutSeconds;
    return true;
#endif
}

bool Network::isAvailable()
{
#ifdef __SWITCH__
    NifmInternetConnectionStatus status = NifmInternetConnectionStatus_ConnectingUnknown1;
    if (R_FAILED(nifmGetInternetConnectionStatus(nullptr, nullptr, &status)))
        return false;
    return status == NifmInternetConnectionStatus_Connected;
#else
    return true;
#endif
}

} // namespace sf
