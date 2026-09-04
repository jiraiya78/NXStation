#pragma once

#include <atomic>
#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>

namespace sf::ui {

/** Full-screen cloud backup upload progress. */
class CloudSyncView : public brls::Box {
public:
    CloudSyncView();
    ~CloudSyncView() override;

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void frame(brls::FrameContext* ctx) override;

    static void present(std::function<void(bool success)> onFinished = nullptr);

private:
    void startSync();

    bool running_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(false);
    std::function<void(bool success)> onFinished_;

    brls::Label* stageLabel_ = nullptr;
    brls::Label* detailLabel_ = nullptr;

    std::atomic<int> progressPercent_{0};
    std::string stageText_;
    std::mutex stageMutex_;
    std::atomic<bool> stageChanged_{false};
    int lastShownPercent_ = -1;
};

class CloudSyncActivity : public brls::Activity {
public:
    explicit CloudSyncActivity(CloudSyncView* view)
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
    CloudSyncView* view_ = nullptr;
};

} // namespace sf::ui
