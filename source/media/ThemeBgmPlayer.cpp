#include "media/ThemeBgmPlayer.hpp"

#include "media/SwitchAudout.hpp"
#include "util/AudioWav.hpp"
#include "util/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(SF_HAS_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#endif

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace sf::audio {

namespace {

constexpr size_t kPushChunkFrames = 1024;
constexpr size_t kBacklogFrames = 12288;

std::atomic<bool> gQuit{false};
std::atomic<bool> gThreadRunning{false};

#if defined(__SWITCH__)
Thread gThread{};
bool gThreadStarted = false;
#else
std::thread gThread;
#endif

std::string gPath;

int bgmThreadPriority()
{
#if defined(__SWITCH__)
    s32 prio = 0x2C;
    if (R_FAILED(svcGetThreadPriority(&prio, CUR_THREAD_HANDLE)))
        prio = 0x2C;
    return std::max(0x18, static_cast<int>(prio) - 1);
#else
    return 0;
#endif
}

void joinWorker()
{
    if (!gThreadRunning.load())
        return;

    gQuit = true;
#if defined(__SWITCH__)
    if (gThreadStarted) {
        threadWaitForExit(&gThread);
        threadClose(&gThread);
        gThreadStarted = false;
    }
#else
    if (gThread.joinable())
        gThread.join();
#endif
    gThreadRunning = false;
    gQuit = false;
}

void pushStereoChunk(const int16_t* stereo, size_t frames, float volume)
{
    if (!SwitchAudout::ready() || frames == 0 || !stereo || volume <= 0.001f)
        return;
    SwitchAudout::pushBgm(stereo, frames, volume);
}

void streamWavLoop(const std::vector<int16_t>& pcm, int channels, float volume)
{
    if (pcm.empty() || channels <= 0)
        return;

    std::vector<int16_t> stereo;
    size_t framePos = 0;
    const size_t totalFrames = pcm.size() / static_cast<size_t>(channels);

    while (!gQuit.load()) {
        while (!gQuit.load() && SwitchAudout::bgmQueuedFrames() > kBacklogFrames)
            std::this_thread::sleep_for(std::chrono::milliseconds(4));

        if (gQuit.load())
            break;

        stereo.clear();
        stereo.reserve(kPushChunkFrames * SwitchAudout::kChannels);
        for (size_t i = 0; i < kPushChunkFrames && !gQuit.load(); ++i) {
            if (framePos >= totalFrames)
                framePos = 0;

            if (channels == 1) {
                const int16_t s = pcm[framePos++];
                stereo.push_back(s);
                stereo.push_back(s);
            } else {
                stereo.push_back(pcm[framePos * 2]);
                stereo.push_back(pcm[framePos * 2 + 1]);
                ++framePos;
            }
        }

        pushStereoChunk(stereo.data(), stereo.size() / SwitchAudout::kChannels, volume);
    }
}

#if defined(SF_HAS_FFMPEG)

std::string toFFmpegUrl(const std::string& path)
{
    if (path.compare(0, 5, "file:") == 0)
        return path;
    return "file:" + path;
}

bool decodeFileOnce(const std::string& path, float volume)
{
    AVFormatContext* fmt = nullptr;
    const std::string url = toFFmpegUrl(path);
    if (avformat_open_input(&fmt, url.c_str(), nullptr, nullptr) < 0) {
        SF_LOG_W("BGM", "FFmpeg open failed: %s", url.c_str());
        return false;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    int streamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    AVStream* stream = fmt->streams[streamIndex];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        return false;
    }

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        avformat_close_input(&fmt);
        return false;
    }

    if (avcodec_parameters_to_context(ctx, stream->codecpar) < 0) {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }

    SwrContext* swr = nullptr;
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout inLayout;
    av_channel_layout_copy(&inLayout, &ctx->ch_layout);
    if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_S16, SwitchAudout::kSampleRate,
                            &inLayout, ctx->sample_fmt, ctx->sample_rate, 0, nullptr) < 0
        || swr_init(swr) < 0) {
        av_channel_layout_uninit(&inLayout);
        swr_free(&swr);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }
    av_channel_layout_uninit(&inLayout);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    std::vector<int16_t> pcm;
    bool ok = true;

    while (!gQuit.load() && av_read_frame(fmt, packet) >= 0) {
        if (packet->stream_index != streamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(ctx, packet) < 0) {
            av_packet_unref(packet);
            ok = false;
            break;
        }
        av_packet_unref(packet);

        while (!gQuit.load() && avcodec_receive_frame(ctx, frame) == 0) {
            while (!gQuit.load() && SwitchAudout::bgmQueuedFrames() > kBacklogFrames)
                std::this_thread::sleep_for(std::chrono::milliseconds(4));

            const int outSamples =
                av_rescale_rnd(swr_get_delay(swr, ctx->sample_rate) + frame->nb_samples,
                               SwitchAudout::kSampleRate, ctx->sample_rate, AV_ROUND_UP);
            if (outSamples <= 0)
                continue;

            pcm.resize(static_cast<size_t>(outSamples) * SwitchAudout::kChannels);
            uint8_t* outData[1] = {reinterpret_cast<uint8_t*>(pcm.data())};
            const int converted = swr_convert(swr, outData, outSamples,
                                              const_cast<const uint8_t**>(frame->data),
                                              frame->nb_samples);
            if (converted > 0)
                pushStereoChunk(pcm.data(), static_cast<size_t>(converted), volume);
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    return ok;
}

#endif

void workerMain()
{
    const std::string path = gPath;
    ThemeBgmPlayer& player = ThemeBgmPlayer::instance();
    if (path.empty() || !player.enabled() || !SwitchAudout::ready()) {
        SwitchAudout::stopBgm();
        return;
    }

    const float volume = player.volume();
    SwitchAudout::startBgm();

    const bool isWav = path.size() >= 4
                       && (path.compare(path.size() - 4, 4, ".wav") == 0
                           || path.compare(path.size() - 4, 4, ".WAV") == 0);

    if (isWav) {
        std::vector<int16_t> pcm;
        int channels = 0;
        if (!loadWavFile(path, &pcm, &channels)) {
            SF_LOG_W("BGM", "Failed to load WAV: %s", path.c_str());
            SwitchAudout::stopBgm();
            return;
        }
        SF_LOG_I("BGM", "Playing WAV loop: %s", path.c_str());
        streamWavLoop(pcm, channels, volume);
        SwitchAudout::stopBgm();
        return;
    }

#if defined(SF_HAS_FFMPEG)
    SF_LOG_I("BGM", "Playing loop: %s", path.c_str());
    while (!gQuit.load()) {
        if (!decodeFileOnce(path, volume))
            break;
    }
    SwitchAudout::stopBgm();
#else
    SF_LOG_W("BGM", "FFmpeg unavailable — cannot play %s", path.c_str());
    SwitchAudout::stopBgm();
#endif
}

#if defined(__SWITCH__)
void workerEntry(void*)
{
    workerMain();
}
#endif

void startWorker()
{
    joinWorker();
    if (gPath.empty())
        return;

    gQuit = false;
    gThreadRunning = true;

#if defined(__SWITCH__)
    constexpr size_t kStack = 512 * 1024;
    if (R_FAILED(threadCreate(&gThread, workerEntry, nullptr, nullptr, kStack, bgmThreadPriority(),
                              -2))) {
        gThreadRunning = false;
        SF_LOG_E("BGM", "threadCreate failed");
        return;
    }
    if (R_FAILED(threadStart(&gThread))) {
        threadClose(&gThread);
        gThreadRunning = false;
        SF_LOG_E("BGM", "threadStart failed");
        return;
    }
    gThreadStarted = true;
#else
    gThread = std::thread(workerMain);
#endif
}

} // namespace

ThemeBgmPlayer& ThemeBgmPlayer::instance()
{
    static ThemeBgmPlayer player;
    return player;
}

void ThemeBgmPlayer::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled_)
        stop();
    else if (!gPath.empty())
        reload(gPath);
}

void ThemeBgmPlayer::setVolume(float volume)
{
    volume_ = std::clamp(volume, 0.f, 1.f);
    SwitchAudout::setBgmVolume(volume_);
}

void ThemeBgmPlayer::reload(const std::string& path)
{
    gPath = path;
    stop();

#if !defined(__SWITCH__)
    (void)path;
    return;
#endif

    if (!enabled_ || path.empty() || !SwitchAudout::ready()) {
        SF_LOG_I("BGM", "Stopped (enabled=%d ready=%d path=%s)", enabled_ ? 1 : 0,
                 SwitchAudout::ready() ? 1 : 0, path.c_str());
        return;
    }

    startWorker();
}

void ThemeBgmPlayer::stop()
{
    joinWorker();
    SwitchAudout::stopBgm();
}

void ThemeBgmPlayer::shutdown()
{
    stop();
    gPath.clear();
}

} // namespace sf::audio
