#pragma once

#include <atomic>
#include <borealis.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace sf::ui {

enum class LibraryScanScope {
    AllSystems,
    SingleSystem,
};

/** Full-screen ROM library scan progress (first launch or manual Scan Games). */
class LibraryScanView : public brls::Box {
public:
    LibraryScanView(LibraryScanScope scope, std::string systemId = {}, bool forceRescan = false);
    ~LibraryScanView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    void setOnComplete(std::function<void()> callback) { onComplete_ = std::move(callback); }

    /** When false, scan does not flip library_scan_completed (reload after NRO handoff). */
    void setMarkLibraryCompleted(bool v) { markLibraryCompleted_ = v; }

    static void presentAll(bool forceRescan = false);
    static void presentSystem(const std::string& systemId, bool forceRescan = true,
                              std::function<void()> onFinished = nullptr);

private:
    void startScan();
    void onScanFinished();

    LibraryScanScope scope_;
    std::string systemId_;
    bool forceRescan_ = false;
    bool markLibraryCompleted_ = true;
    bool running_ = false;

    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::function<void()> onComplete_;

    brls::Label* systemLabel_ = nullptr;
    brls::Label* progressLabel_ = nullptr;

    // Progress is written from the scan worker thread without going through brls::sync,
    // since a fast in-memory scan can queue hundreds of sync callbacks within a single
    // frame — only the last one would ever be visible. Polling from frame() instead shows
    // genuinely incremental progress at the display's refresh rate.
    std::atomic<size_t> progressCurrent_{0};
    std::atomic<size_t> progressTotal_{0};
    std::mutex nameMutex_;
    std::string progressSystemName_;
    std::atomic<bool> nameChanged_{false};
    size_t lastShownCurrent_ = static_cast<size_t>(-1);
    size_t lastShownTotal_ = static_cast<size_t>(-1);
};

class LibraryScanActivity : public brls::Activity {
public:
    explicit LibraryScanActivity(LibraryScanView* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onContentAvailable() override
    {
        if (view_)
            brls::Application::giveFocus(view_);
    }

private:
    LibraryScanView* view_ = nullptr;
};

} // namespace sf::ui
