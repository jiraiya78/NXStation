#include "media/SwitchAudout.hpp"

#include "util/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>

#if defined(__SWITCH__)
#include <malloc.h>
#include <switch.h>
#endif

namespace sf::audio {

namespace {

#if defined(__SWITCH__)

// 1024 frames = 4096 bytes, which satisfies the 0x1000 buffer alignment audout requires.
constexpr size_t kChunkFrames = 1024;
constexpr int kBufferCount = 4;

constexpr size_t kSfxMaxFrames = SwitchAudout::kSampleRate;
constexpr size_t kStreamPrimeFrames = 4096;
constexpr size_t kStreamMaxFrames = 16384;
constexpr size_t kBgmPrimeFrames = 4096;
constexpr size_t kBgmMaxFrames = 48000 * 120;

// Ramp the preview stream in/out over ~5 ms so start, stop and underruns never step.
constexpr size_t kFadeFrames = 256;
constexpr size_t kThreadStackSize = 32 * 1024;

struct Slot {
    AudioOutBuffer buf{};
    uint8_t* mem = nullptr;
    bool queued = false;
};

struct State {
    bool ready = false;
    Slot slots[kBufferCount];
    size_t chunkBytes = 0;

    std::mutex mutex;
    std::deque<int16_t> sfx;
    std::deque<int16_t> stream;
    std::deque<int16_t> bgm;
    bool streamActive = false;
    bool streamPrimed = false;
    bool bgmActive = false;
    bool bgmPrimed = false;
    float streamGain = 0.f;
    float bgmGain = 0.f;
    float bgmUserVolume = 1.f;
    float tailL = 0.f;
    float tailR = 0.f;

    Thread thread{};
    bool threadStarted = false;
    std::atomic<bool> quit{false};
    std::atomic<size_t> underruns{0};
};

State g;

int16_t clampSample(float v)
{
    return static_cast<int16_t>(std::clamp(v, -32768.f, 32767.f));
}

/** Mix one hardware chunk. Caller holds g.mutex. */
void mixChunkUnlocked(int16_t* out, size_t frames)
{
    if (g.streamActive && !g.streamPrimed
        && g.stream.size() >= kStreamPrimeFrames * SwitchAudout::kChannels)
        g.streamPrimed = true;

    const float gainStep = 1.f / static_cast<float>(kFadeFrames);

    for (size_t f = 0; f < frames; ++f) {
        float left = 0.f;
        float right = 0.f;

        if (g.sfx.size() >= 2) {
            left += static_cast<float>(g.sfx.front());
            g.sfx.pop_front();
            right += static_cast<float>(g.sfx.front());
            g.sfx.pop_front();
        }

        const bool wantStream = g.streamActive && g.streamPrimed;
        if (wantStream && g.stream.size() >= 2) {
            g.streamGain = std::min(1.f, g.streamGain + gainStep);
            g.tailL = static_cast<float>(g.stream.front()) * g.streamGain;
            g.stream.pop_front();
            g.tailR = static_cast<float>(g.stream.front()) * g.streamGain;
            g.stream.pop_front();
            left += g.tailL;
            right += g.tailR;
        } else {
            if (wantStream) {
                // Ran dry: re-prime before resuming instead of stitching in silence.
                g.streamPrimed = false;
                g.underruns.fetch_add(1);
            }
            // Decay whatever the stream last played rather than cutting to zero.
            g.streamGain = std::max(0.f, g.streamGain - gainStep);
            g.tailL *= 0.90f;
            g.tailR *= 0.90f;
            if (std::abs(g.tailL) < 1.f)
                g.tailL = 0.f;
            if (std::abs(g.tailR) < 1.f)
                g.tailR = 0.f;
            left += g.tailL;
            right += g.tailR;
        }

        if (g.bgmActive && !g.bgmPrimed
            && g.bgm.size() >= kBgmPrimeFrames * SwitchAudout::kChannels)
            g.bgmPrimed = true;

        const float bgmDuck = wantStream ? 0.22f : 1.f;
        if (g.bgmActive && g.bgmPrimed && g.bgm.size() >= 2) {
            g.bgmGain = std::min(1.f, g.bgmGain + gainStep);
            left += static_cast<float>(g.bgm.front()) * g.bgmUserVolume * bgmDuck * g.bgmGain;
            g.bgm.pop_front();
            right += static_cast<float>(g.bgm.front()) * g.bgmUserVolume * bgmDuck * g.bgmGain;
            g.bgm.pop_front();
        } else if (g.bgmActive && !g.bgmPrimed) {
            g.bgmGain = std::max(0.f, g.bgmGain - gainStep);
        }

        out[f * SwitchAudout::kChannels + 0] = clampSample(left);
        out[f * SwitchAudout::kChannels + 1] = clampSample(right);
    }
}

void fillAndAppend(Slot& slot)
{
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        mixChunkUnlocked(reinterpret_cast<int16_t*>(slot.mem), kChunkFrames);
    }

    slot.buf.data_size = g.chunkBytes;
    slot.buf.data_offset = 0;
    if (R_SUCCEEDED(audoutAppendAudioOutBuffer(&slot.buf)))
        slot.queued = true;
}

void markReleased(AudioOutBuffer* released)
{
    for (Slot& slot : g.slots) {
        if (&slot.buf == released) {
            slot.queued = false;
            return;
        }
    }
}

bool anyQueued()
{
    for (const Slot& slot : g.slots) {
        if (slot.queued)
            return true;
    }
    return false;
}

/** Reclaim buffers the driver actually handed back — never assume ownership. */
void reclaimReleased(uint64_t timeoutNs)
{
    AudioOutBuffer* released = nullptr;
    u32 count = 0;

    if (anyQueued() && R_SUCCEEDED(audoutWaitPlayFinish(&released, &count, timeoutNs)) && count > 0
        && released)
        markReleased(released);

    while (true) {
        released = nullptr;
        count = 0;
        if (R_FAILED(audoutGetReleasedAudioOutBuffer(&released, &count)) || count == 0 || !released)
            break;
        markReleased(released);
    }
}

void audioThreadFunc(void*)
{
    while (!g.quit.load()) {
        reclaimReleased(5000000ULL);

        bool appended = false;
        for (Slot& slot : g.slots) {
            if (!slot.queued) {
                fillAndAppend(slot);
                appended = true;
            }
        }

        if (!appended && !anyQueued())
            svcSleepThread(2000000ULL);
    }

    reclaimReleased(UINT64_MAX);
}

/** Run just above the UI thread so mixing never waits on rendering. */
int audioThreadPriority()
{
    s32 prio = 0x2C;
    if (R_FAILED(svcGetThreadPriority(&prio, CUR_THREAD_HANDLE)))
        prio = 0x2C;
    return std::max(0x18, static_cast<int>(prio) - 1);
}

#endif

} // namespace

void SwitchAudout::init()
{
#if defined(__SWITCH__)
    if (g.ready)
        return;

    if (R_FAILED(audoutInitialize())) {
        SF_LOG_W("Audio", "audoutInitialize failed");
        return;
    }
    if (R_FAILED(audoutStartAudioOut())) {
        SF_LOG_W("Audio", "audoutStartAudioOut failed");
        audoutExit();
        return;
    }

    g.chunkBytes = kChunkFrames * kChannels * sizeof(int16_t);
    for (Slot& slot : g.slots) {
        slot.mem = static_cast<uint8_t*>(memalign(0x1000, g.chunkBytes));
        if (!slot.mem) {
            SF_LOG_E("Audio", "memalign failed");
            return;
        }
        std::memset(slot.mem, 0, g.chunkBytes);
        slot.buf.buffer = slot.mem;
        slot.buf.buffer_size = g.chunkBytes;
        slot.buf.data_size = 0;
        slot.buf.data_offset = 0;
        slot.queued = false;
    }

    g.ready = true;
    g.quit = false;

    const int prio = audioThreadPriority();
    if (R_FAILED(threadCreate(&g.thread, audioThreadFunc, nullptr, nullptr, kThreadStackSize, prio,
                              -2))) {
        SF_LOG_E("Audio", "audio threadCreate failed");
        g.ready = false;
        return;
    }
    if (R_FAILED(threadStart(&g.thread))) {
        SF_LOG_E("Audio", "audio threadStart failed");
        threadClose(&g.thread);
        g.ready = false;
        return;
    }

    g.threadStarted = true;
    SF_LOG_I("Audio", "SwitchAudout ready (%d x %zu frames, prio=0x%X)", kBufferCount, kChunkFrames,
             prio);
#endif
}

void SwitchAudout::shutdown()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    g.quit = true;
    if (g.threadStarted) {
        threadWaitForExit(&g.thread);
        threadClose(&g.thread);
        g.threadStarted = false;
    }

    audoutStopAudioOut();
    audoutExit();

    for (Slot& slot : g.slots) {
        free(slot.mem);
        slot.mem = nullptr;
        slot.queued = false;
    }

    g.ready = false;
    SF_LOG_I("Audio", "SwitchAudout stopped (underruns=%zu)", g.underruns.load());
#endif
}

bool SwitchAudout::ready()
{
#if defined(__SWITCH__)
    return g.ready;
#else
    return false;
#endif
}

void SwitchAudout::startStream()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.stream.clear();
    g.streamActive = true;
    g.streamPrimed = false;
    g.streamGain = 0.f;
#endif
}

void SwitchAudout::stopStream()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.stream.clear();
    g.streamActive = false;
    g.streamPrimed = false;
#endif
}

void SwitchAudout::clearStream()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.stream.clear();
    g.streamPrimed = false;
#endif
}

size_t SwitchAudout::streamQueuedFrames()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return 0;

    std::lock_guard<std::mutex> lock(g.mutex);
    return g.stream.size() / kChannels;
#else
    return 0;
#endif
}

void SwitchAudout::pushStream(const int16_t* stereoFrames, size_t frameCount, float volume)
{
#if defined(__SWITCH__)
    if (!g.ready || frameCount == 0 || !stereoFrames || volume <= 0.001f)
        return;

    const size_t count = frameCount * kChannels;
    std::lock_guard<std::mutex> lock(g.mutex);
    if (!g.streamActive)
        return;

    const size_t maxSamples = kStreamMaxFrames * kChannels;
    if (g.stream.size() + count > maxSamples)
        return;

    if (volume >= 0.999f) {
        g.stream.insert(g.stream.end(), stereoFrames, stereoFrames + count);
    } else {
        for (size_t i = 0; i < count; ++i)
            g.stream.push_back(
                static_cast<int16_t>(static_cast<float>(stereoFrames[i]) * volume));
    }
#else
    (void)stereoFrames;
    (void)frameCount;
    (void)volume;
#endif
}

void SwitchAudout::playSfx(const int16_t* mono, size_t sampleCount, float volume)
{
#if defined(__SWITCH__)
    if (!g.ready || sampleCount == 0 || !mono || volume <= 0.001f)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    if (g.sfx.size() > kSfxMaxFrames * kChannels)
        g.sfx.clear();

    for (size_t i = 0; i < sampleCount; ++i) {
        const int16_t s = static_cast<int16_t>(static_cast<float>(mono[i]) * volume);
        g.sfx.push_back(s);
        g.sfx.push_back(s);
    }
#else
    (void)mono;
    (void)sampleCount;
    (void)volume;
#endif
}

void SwitchAudout::startBgm()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.bgm.clear();
    g.bgmActive = true;
    g.bgmPrimed = false;
    g.bgmGain = 0.f;
#endif
}

void SwitchAudout::stopBgm()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.bgm.clear();
    g.bgmActive = false;
    g.bgmPrimed = false;
    g.bgmGain = 0.f;
#endif
}

void SwitchAudout::clearBgm()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return;

    std::lock_guard<std::mutex> lock(g.mutex);
    g.bgm.clear();
    g.bgmPrimed = false;
#endif
}

size_t SwitchAudout::bgmQueuedFrames()
{
#if defined(__SWITCH__)
    if (!g.ready)
        return 0;

    std::lock_guard<std::mutex> lock(g.mutex);
    return g.bgm.size() / kChannels;
#else
    return 0;
#endif
}

void SwitchAudout::pushBgm(const int16_t* stereoFrames, size_t frameCount, float volume)
{
#if defined(__SWITCH__)
    if (!g.ready || frameCount == 0 || !stereoFrames || volume <= 0.001f)
        return;

    const size_t count = frameCount * kChannels;
    std::lock_guard<std::mutex> lock(g.mutex);
    if (!g.bgmActive)
        return;

    const size_t maxSamples = kBgmMaxFrames * kChannels;
    if (g.bgm.size() + count > maxSamples)
        return;

    if (volume >= 0.999f) {
        g.bgm.insert(g.bgm.end(), stereoFrames, stereoFrames + count);
    } else {
        for (size_t i = 0; i < count; ++i)
            g.bgm.push_back(static_cast<int16_t>(static_cast<float>(stereoFrames[i]) * volume));
    }
#else
    (void)stereoFrames;
    (void)frameCount;
    (void)volume;
#endif
}

void SwitchAudout::setBgmVolume(float volume)
{
#if defined(__SWITCH__)
    std::lock_guard<std::mutex> lock(g.mutex);
    g.bgmUserVolume = std::clamp(volume, 0.f, 1.f);
#else
    (void)volume;
#endif
}

} // namespace sf::audio
