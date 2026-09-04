#include "ui/GoogleDriveLinkView.hpp"
#include "app/AppState.hpp"
#include "cloud/GoogleDriveClient.hpp"
#include "ui/ThemeManager.hpp"
#include "util/ActionLog.hpp"
#include "util/QrCodeUtil.hpp"

#include <ctime>
#include <mutex>

namespace sf::ui {

GoogleDriveLinkView::GoogleDriveLinkView()
{
    auto& theme = ThemeManager::instance();
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setBackgroundColor(theme.color("brls/background"));
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setFocusable(true);
    this->setHideHighlight(true);

    messageLabel_ = new brls::Label();
    messageLabel_->setFontSize(26);
    messageLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    messageLabel_->setTextColor(theme.color("brls/text"));
    messageLabel_->setText("Link Google Account");
    this->addView(messageLabel_);

    qrImage_ = new brls::Image();
    qrImage_->setScalingType(brls::ImageScalingType::FIT);
    qrImage_->setImageAlign(brls::ImageAlignment::CENTER);
    qrImage_->setWidth(300.f);
    qrImage_->setHeight(300.f);
    qrImage_->setMarginTop(16);
    this->addView(qrImage_);

    codeLabel_ = new brls::Label();
    codeLabel_->setFontSize(36);
    codeLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    codeLabel_->setTextColor(theme.color("brls/text"));
    codeLabel_->setMarginTop(12);
    codeLabel_->setText("");
    this->addView(codeLabel_);

    hintLabel_ = new brls::Label();
    hintLabel_->setFontSize(20);
    hintLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    hintLabel_->setTextColor(theme.color("nxstation/detail_text"));
    hintLabel_->setMarginTop(16);
    hintLabel_->setSingleLine(false);
    hintLabel_->setText("Requesting sign-in code…");
    this->addView(hintLabel_);

    this->registerAction(
        "Cancel", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
            cancelled_ = true;
            if (onFinished_)
                onFinished_(false);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
}

GoogleDriveLinkView::~GoogleDriveLinkView()
{
    *alive_ = false;
}

void GoogleDriveLinkView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    *alive_ = true;
    brls::Application::giveFocus(this);
    if (!running_)
        beginSignIn();
}

void GoogleDriveLinkView::willDisappear(bool resetState)
{
    *alive_ = false;
    brls::Box::willDisappear(resetState);
}

void GoogleDriveLinkView::frame(brls::FrameContext* ctx)
{
    if (messageChanged_.exchange(false) && messageLabel_) {
        std::lock_guard<std::mutex> lock(messageMutex_);
        messageLabel_->setText(messageText_);
    }

    if (running_ && !cancelled_ && !deviceCode_.empty()) {
        const std::time_t now = std::time(nullptr);
        if (now >= expiresAt_) {
            running_ = false;
            brls::Application::notify("Google sign-in timed out");
            if (onFinished_)
                onFinished_(false);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
        } else if (now >= nextPollAt_) {
            nextPollAt_ = now + pollIntervalSeconds_;
            pollSignIn();
        }
    }

    brls::Box::frame(ctx);
}

void GoogleDriveLinkView::applySignInUi(const std::string& message, const std::string& userCode,
                                        const std::string& qrUrl, const std::string& deviceCode,
                                        std::time_t expiresAt, int pollIntervalSeconds)
{
    deviceCode_ = deviceCode;
    expiresAt_ = expiresAt;
    pollIntervalSeconds_ = pollIntervalSeconds;
    nextPollAt_ = std::time(nullptr) + pollIntervalSeconds_;

    {
        std::lock_guard<std::mutex> lock(messageMutex_);
        messageText_ = message;
    }
    messageChanged_.store(true);

    if (codeLabel_)
        codeLabel_->setText(userCode);

    qrPngBuffer_ = sf::encodeQrPng(qrUrl, 6, 4);
    if (qrImage_ && !qrPngBuffer_.empty())
        qrImage_->setImageFromMem(qrPngBuffer_.data(), static_cast<int>(qrPngBuffer_.size()));

    if (hintLabel_)
        hintLabel_->setText("Scan QR on your phone, or enter the code at google.com/device\n(B to cancel)");
}

void GoogleDriveLinkView::beginSignIn()
{
    running_ = true;
    auto alive = alive_;
    AppState::instance().pool().enqueue([this, alive]() {
        sf::cloud::GoogleDriveClient drive;
        sf::cloud::DeviceSignInData data;
        const bool ok = drive.requestDeviceSignIn(data);
        const std::string err = drive.lastError();
        brls::sync([this, alive, ok, data, err]() mutable {
            if (!*alive)
                return;
            if (!ok) {
                running_ = false;
                brls::Application::notify("Sign-in failed: " + err);
                if (onFinished_)
                    onFinished_(false);
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return;
            }
            applySignInUi(data.message, data.userCode, data.qrUrl, data.deviceCode, data.expiresAt,
                          data.pollIntervalSeconds);
        });
    });
}

void GoogleDriveLinkView::pollSignIn()
{
    const std::string code = deviceCode_;
    auto alive = alive_;
    AppState::instance().pool().enqueue([this, alive, code]() {
        sf::cloud::GoogleDriveClient drive;
        const bool linked = drive.pollDeviceSignIn(code);
        if (!linked)
            return;

        brls::sync([this, alive]() {
            if (!*alive)
                return;
            running_ = false;
            brls::Application::notify("Google account linked");
            if (onFinished_)
                onFinished_(true);
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
        });
    });
}

void GoogleDriveLinkView::present(std::function<void(bool linked)> onFinished)
{
    auto* view = new GoogleDriveLinkView();
    view->onFinished_ = std::move(onFinished);
    brls::Application::pushActivity(new GoogleDriveLinkActivity(view));
}

} // namespace sf::ui
