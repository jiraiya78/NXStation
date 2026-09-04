#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sf {

/** Streams PCM preview audio via the shared Switch audout queue. */
class PreviewAudio {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    void setVolume(float volume);
    float volume() const { return volume_; }

    void start();
    void stop();
    void clear();

    /** Called from the decode thread. */
    void pushPcm(const int16_t* samples, size_t frameCount);

    /** Frames still buffered for playback — the decoder throttles on this. */
    size_t queuedFrames() const;

private:
    bool enabled_ = true;
    float volume_ = 0.75f;
    std::atomic<bool> active_{false};
};

} // namespace sf
