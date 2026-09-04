#pragma once

#include <cstddef>
#include <cstdint>

namespace sf::audio {

/**
 * Single Switch audout session shared by UI sfx and the video preview stream.
 *
 * A dedicated audio thread owns the hardware buffers and blocks on buffer release, so
 * refills never depend on the render loop finishing a frame on time.
 */
class SwitchAudout {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    static void init();
    static void shutdown();
    static bool ready();

    /** Begin accepting preview PCM; playback starts once enough is buffered. */
    static void startStream();

    /** Fade the stream out and discard anything still queued. */
    static void stopStream();

    static void clearStream();
    static void pushStream(const int16_t* stereoFrames, size_t frameCount, float volume);

    /** Frames still waiting to be played — used by the decoder to pace itself. */
    static size_t streamQueuedFrames();

    /** Mono 16-bit samples duplicated to stereo and mixed on top of the stream. */
    static void playSfx(const int16_t* mono, size_t sampleCount, float volume);

    /** Looping theme background music (mixed under preview audio, ducked when preview plays). */
    static void startBgm();
    static void stopBgm();
    static void clearBgm();
    static void pushBgm(const int16_t* stereoFrames, size_t frameCount, float volume);
    static size_t bgmQueuedFrames();
    static void setBgmVolume(float volume);
};

} // namespace sf::audio
