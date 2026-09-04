#pragma once

#include "media/PreviewAudio.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace sf {

struct VideoFrame {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
    double pts = 0.0;
};

class VideoPlayer {
public:
    static constexpr int kMaxPreviewWidth = 480;
    static constexpr int kMaxPreviewHeight = 320;

    VideoPlayer();
    ~VideoPlayer();

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    void setAudioEnabled(bool enabled) { audioEnabled_ = enabled; }
    bool audioEnabled() const { return audioEnabled_; }

    void setAudioVolume(float volume) { previewAudio_.setVolume(volume); }

    void setHoverDelaySeconds(float s) { hoverDelaySeconds_ = s; }

    /** When false, the decode thread exits at EOF instead of seeking back to the start. */
    void setLoopPlayback(bool loop) { loopPlayback_ = loop; }
    bool loopPlayback() const { return loopPlayback_.load(); }

    void onSelectionChanged(const std::string& videoPath);
    void tick(float delta);
    bool hasFrame() const;
    bool tryConsumeFrame(VideoFrame& out);

    /** Hand a consumed frame back so the decoder can reuse its pixel buffer. */
    void recycleFrame(VideoFrame&& frame);

    void stop();
    void shutdown();

    bool playing() const { return playing_.load(); }
    bool shouldStopDecode() const;

    PreviewAudio& previewAudio() { return previewAudio_; }

private:
#if defined(__SWITCH__)
    static constexpr size_t kDecodeStackSize = 0x200000;
    static void decodeThreadEntry(void* arg);
#endif
    void decodeLoop(std::string path, uint64_t generation);
    void runDecode(const std::string& path, uint64_t generation);
    void joinDecodeThread();
    void startDecodeThread(const std::string& path, uint64_t generation);
    void takeSpareBuffer(std::vector<uint8_t>& out);

    bool enabled_ = true;
    bool audioEnabled_ = true;
    float hoverDelaySeconds_ = 1.0f;

    std::string pendingPath_;
    float hoverTimer_ = 0.f;
    bool waitingHover_ = false;

    std::atomic<bool> stopDecode_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> loopPlayback_{true};
    std::atomic<uint64_t> generation_{0};

    PreviewAudio previewAudio_;

#if defined(__SWITCH__)
    Thread decodeThread_{};
    bool decodeThreadActive_ = false;
#else
    std::thread decodeThread_;
#endif

    mutable std::mutex frameMutex_;
    VideoFrame latestFrame_;
    std::vector<uint8_t> spareBuffer_;
    bool hasNewFrame_ = false;
};

} // namespace sf
