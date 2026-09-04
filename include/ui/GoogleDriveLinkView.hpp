#pragma once

#include <atomic>
#include <borealis.hpp>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sf::ui {

/** Google OAuth device-flow linking dialog with QR code. */
class GoogleDriveLinkView : public brls::Box {
public:
    GoogleDriveLinkView();
    ~GoogleDriveLinkView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    static void present(std::function<void(bool linked)> onFinished = nullptr);

private:
    void beginSignIn();
    void pollSignIn();
    void applySignInUi(const std::string& message, const std::string& userCode,
                       const std::string& qrUrl, const std::string& deviceCode,
                       std::time_t expiresAt, int pollIntervalSeconds);

    bool running_ = false;
    bool cancelled_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::function<void(bool linked)> onFinished_;

    brls::Label* messageLabel_ = nullptr;
    brls::Image* qrImage_ = nullptr;
    brls::Label* codeLabel_ = nullptr;
    brls::Label* hintLabel_ = nullptr;

    std::vector<uint8_t> qrPngBuffer_;

    std::string deviceCode_;
    std::time_t expiresAt_ = 0;
    int pollIntervalSeconds_ = 5;
    std::time_t nextPollAt_ = 0;

    std::string messageText_;
    std::mutex messageMutex_;
    std::atomic<bool> messageChanged_{false};
};

class GoogleDriveLinkActivity : public brls::Activity {
public:
    explicit GoogleDriveLinkActivity(GoogleDriveLinkView* view)
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
    GoogleDriveLinkView* view_ = nullptr;
};

} // namespace sf::ui
