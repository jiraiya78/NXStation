#include "media/VideoPlayer.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <thread>
#include <algorithm>

#if defined(__SWITCH__)
#include <switch.h>
#endif

#if defined(SF_HAS_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#endif

namespace sf {

namespace {

#if defined(SF_HAS_FFMPEG)
const char* ffmpegError(int err, char (&buf)[AV_ERROR_MAX_STRING_SIZE])
{
    if (av_strerror(err, buf, sizeof(buf)) < 0)
        std::snprintf(buf, sizeof(buf), "error %d", err);
    return buf;
}

void ffmpegLogBridge(void* avcl, int level, const char* fmt, va_list args)
{
    (void)avcl;
    if (level > AV_LOG_WARNING)
        return;

    char line[512];
    std::vsnprintf(line, sizeof(line), fmt, args);

    // FFmpeg messages carry their own trailing newline.
    size_t len = std::strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
    if (len == 0)
        return;

    if (level <= AV_LOG_ERROR)
        SF_LOG_E("FFmpeg", "%s", line);
    else
        SF_LOG_W("FFmpeg", "%s", line);
}

/**
 * FFmpeg resolves everything before the first ':' as a protocol name, so devkitPro
 * devoptab mounts ("sdmc:/...", "romfs:/...") look like an unknown protocol and never
 * reach the file handler. Forcing the "file:" scheme makes FFmpeg strip it and pass the
 * remainder straight to open(), where the devoptab resolves the mount as usual.
 */
std::string toFFmpegUrl(const std::string& path)
{
    if (path.compare(0, 5, "file:") == 0)
        return path;
    return "file:" + path;
}

bool isValidPixelFormat(AVPixelFormat fmt)
{
    return fmt != AV_PIX_FMT_NONE && av_pix_fmt_desc_get(fmt) != nullptr;
}

/**
 * Some MP4 previews report "unspecified pixel format" until the first frame is decoded.
 * swscale asserts on AV_PIX_FMT_NONE — resolve the real format before creating SwsContext.
 */
AVPixelFormat resolveVideoPixelFormat(AVFormatContext* fmt, AVCodecContext* codecCtx, int streamIndex,
                                      AVPacket* pkt, AVFrame* frame)
{
    if (isValidPixelFormat(codecCtx->pix_fmt))
        return codecCtx->pix_fmt;

    AVStream* stream = fmt->streams[streamIndex];
    if (stream && stream->codecpar && stream->codecpar->format != AV_PIX_FMT_NONE) {
        const auto fmtFromPar = static_cast<AVPixelFormat>(stream->codecpar->format);
        if (isValidPixelFormat(fmtFromPar)) {
            codecCtx->pix_fmt = fmtFromPar;
            return fmtFromPar;
        }
    }

    const int64_t startPos = stream ? stream->start_time : 0;
    if (startPos != AV_NOPTS_VALUE)
        av_seek_frame(fmt, streamIndex, startPos, AVSEEK_FLAG_BACKWARD);
    else
        av_seek_frame(fmt, streamIndex, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);

    AVPixelFormat found = AV_PIX_FMT_NONE;
    for (int attempt = 0; attempt < 256 && found == AV_PIX_FMT_NONE; ++attempt) {
        if (av_read_frame(fmt, pkt) < 0)
            break;
        if (pkt->stream_index != streamIndex) {
            av_packet_unref(pkt);
            continue;
        }
        if (avcodec_send_packet(codecCtx, pkt) == 0) {
            while (avcodec_receive_frame(codecCtx, frame) == 0) {
                const auto frameFmt = static_cast<AVPixelFormat>(frame->format);
                if (isValidPixelFormat(frameFmt)) {
                    found = frameFmt;
                    codecCtx->pix_fmt = frameFmt;
                    break;
                }
            }
        }
        av_packet_unref(pkt);
    }

    if (startPos != AV_NOPTS_VALUE)
        av_seek_frame(fmt, streamIndex, startPos, AVSEEK_FLAG_BACKWARD);
    else
        av_seek_frame(fmt, streamIndex, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);
    return found;
}

SwsContext* createScaleContext(int srcW, int srcH, AVPixelFormat srcFmt, int dstW, int dstH)
{
    if (!isValidPixelFormat(srcFmt) || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return nullptr;
    return sws_getContext(srcW, srcH, srcFmt, dstW, dstH, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR,
                          nullptr, nullptr, nullptr);
}
#endif

void packTightRgba(const uint8_t* src, int srcStride, int width, int height, std::vector<uint8_t>& out)
{
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    out.resize(rowBytes * static_cast<size_t>(height));
    if (srcStride == static_cast<int>(rowBytes)) {
        std::memcpy(out.data(), src, rowBytes * static_cast<size_t>(height));
        return;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(out.data() + static_cast<size_t>(y) * rowBytes,
                    src + static_cast<size_t>(y) * static_cast<size_t>(srcStride), rowBytes);
    }
}

#if defined(SF_HAS_FFMPEG)
struct PlaybackClock {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start{};
    double basePts = NAN;
    bool started = false;

    void reset()
    {
        started = false;
        basePts = NAN;
    }

    /** Seconds the wall clock is ahead of this PTS — positive means the frame is late. */
    double latenessFor(double pts) const
    {
        if (!started)
            return 0.0;
        if (std::isnan(pts) || pts < 0.0)
            pts = 0.0;
        const double rel = pts - basePts;
        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        return elapsed - rel;
    }

    void waitUntil(double pts)
    {
        if (std::isnan(pts) || pts < 0.0)
            pts = 0.0;
        if (!started) {
            basePts = pts;
            start = Clock::now();
            started = true;
            return;
        }
        const double rel = pts - basePts;
        if (rel < 0.0)
            return;
        const auto target =
            start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(rel));
        const auto now = Clock::now();
        if (target > now)
            std::this_thread::sleep_until(target);
    }
};

/** ~256 ms of decoded preview audio kept queued ahead of the audio device. */
constexpr size_t kAudioBacklogFrames = 12288;

/**
 * Audio decode owns its own thread.
 *
 * The demux thread has to sleep to pace video against the presentation clock, and any sleep
 * there stops audio packets from reaching the device (crackle). Conversely, throttling audio
 * on the demux thread stops video packets from being read (stutter). Splitting the two means
 * neither side can starve the other, so no packets or samples ever have to be dropped.
 */
struct AudioWorker {
    static constexpr size_t kMaxQueuedPackets = 64;

    AVCodecContext* ctx = nullptr;
    SwrContext* swr = nullptr;
    int outRate = PreviewAudio::kSampleRate;
    int outChannels = PreviewAudio::kChannels;
    PreviewAudio* sink = nullptr;

    std::mutex mutex;
    std::condition_variable cv;
    std::deque<AVPacket*> queue;
    bool flushRequested = false;
    std::atomic<bool> quit{false};

    /** Takes ownership of pkt. */
    void submit(AVPacket* pkt)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.size() >= kMaxQueuedPackets) {
                AVPacket* oldest = queue.front();
                queue.pop_front();
                av_packet_free(&oldest);
            }
            queue.push_back(pkt);
        }
        cv.notify_one();
    }

    /** Discard queued packets and flush the decoder on loop/seek. */
    void requestFlush()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            clearQueueUnlocked();
            flushRequested = true;
        }
        cv.notify_one();
    }

    void stop()
    {
        quit = true;
        cv.notify_all();
    }

    void run()
    {
        AVFrame* frame = av_frame_alloc();
        if (!frame)
            return;

        std::vector<int16_t> pcm;

        while (!quit.load()) {
            AVPacket* pkt = nullptr;
            bool doFlush = false;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] { return quit.load() || !queue.empty(); });
                if (quit.load())
                    break;
                doFlush = flushRequested;
                flushRequested = false;
                pkt = queue.front();
                queue.pop_front();
            }

            if (doFlush)
                avcodec_flush_buffers(ctx);

            // Safe to sleep: nothing on the video path waits behind this thread.
            while (!quit.load() && sink->queuedFrames() > kAudioBacklogFrames)
                std::this_thread::sleep_for(std::chrono::milliseconds(4));

            if (quit.load()) {
                av_packet_free(&pkt);
                break;
            }

            if (avcodec_send_packet(ctx, pkt) == 0) {
                while (!quit.load() && avcodec_receive_frame(ctx, frame) == 0) {
                    const int outSamples =
                        av_rescale_rnd(swr_get_delay(swr, ctx->sample_rate) + frame->nb_samples,
                                       outRate, ctx->sample_rate, AV_ROUND_UP);
                    if (outSamples <= 0)
                        continue;
                    pcm.resize(static_cast<size_t>(outSamples) * outChannels);
                    uint8_t* outData[1] = {reinterpret_cast<uint8_t*>(pcm.data())};
                    const int converted =
                        swr_convert(swr, outData, outSamples,
                                    const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                    if (converted > 0)
                        sink->pushPcm(pcm.data(), static_cast<size_t>(converted));
                }
            }
            av_packet_free(&pkt);
        }

        av_frame_free(&frame);

        std::lock_guard<std::mutex> lock(mutex);
        clearQueueUnlocked();
    }

private:
    void clearQueueUnlocked()
    {
        while (!queue.empty()) {
            AVPacket* pkt = queue.front();
            queue.pop_front();
            av_packet_free(&pkt);
        }
    }
};

#if defined(__SWITCH__)
void audioWorkerEntry(void* arg)
{
    static_cast<AudioWorker*>(arg)->run();
}
#endif

/** Guarantees the audio thread is joined before its worker leaves scope. */
struct AudioThreadGuard {
    AudioWorker* worker = nullptr;
    bool running = false;
#if defined(__SWITCH__)
    Thread thread{};
#else
    std::thread thread;
#endif

    ~AudioThreadGuard() { join(); }

    void join()
    {
        if (!running)
            return;
        running = false;
        worker->stop();
#if defined(__SWITCH__)
        threadWaitForExit(&thread);
        threadClose(&thread);
#else
        if (thread.joinable())
            thread.join();
#endif
    }
};

int ffmpegInterrupt(void* opaque)
{
    auto* player = static_cast<VideoPlayer*>(opaque);
    return player->shouldStopDecode() ? 1 : 0;
}
#endif

#if defined(__SWITCH__)
s32 currentThreadPriority()
{
    s32 prio = 0x2C;
    if (R_FAILED(svcGetThreadPriority(&prio, CUR_THREAD_HANDLE)))
        prio = 0x2C;
    return prio;
}

/** Run the decoder just below the caller (UI) thread so a slow decode never starves rendering. */
int decodeThreadPriority()
{
    const int lowered = static_cast<int>(currentThreadPriority()) + 1;
    return lowered > 0x3F ? 0x3F : lowered;
}

/** Audio decode sits above video decode: starving it is audible, a late frame is not. */
int audioDecodeThreadPriority()
{
    return static_cast<int>(currentThreadPriority());
}

constexpr size_t kAudioDecodeStackSize = 0x20000;
#endif

} // namespace

#if defined(__SWITCH__)
struct DecodeJob {
    VideoPlayer* player = nullptr;
    std::string path;
    uint64_t generation = 0;
};

void VideoPlayer::decodeThreadEntry(void* arg)
{
    auto* job = static_cast<DecodeJob*>(arg);
    job->player->decodeLoop(job->path, job->generation);
    delete job;
}
#endif

VideoPlayer::VideoPlayer()
{
#if defined(SF_HAS_FFMPEG)
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(ffmpegLogBridge);
#endif
}

VideoPlayer::~VideoPlayer()
{
    shutdown();
}

bool VideoPlayer::shouldStopDecode() const
{
    return stopDecode_.load();
}

void VideoPlayer::joinDecodeThread()
{
#if defined(__SWITCH__)
    if (decodeThreadActive_) {
        threadWaitForExit(&decodeThread_);
        threadClose(&decodeThread_);
        decodeThreadActive_ = false;
    }
#else
    if (decodeThread_.joinable())
        decodeThread_.join();
#endif
}

void VideoPlayer::startDecodeThread(const std::string& path, uint64_t generation)
{
    joinDecodeThread();
    stopDecode_ = false;

#if defined(__SWITCH__)
    const int prio = decodeThreadPriority();
    auto* job = new DecodeJob{this, path, generation};
    const Result rc =
        threadCreate(&decodeThread_, decodeThreadEntry, job, nullptr, kDecodeStackSize, prio, -2);
    if (R_FAILED(rc)) {
        SF_LOG_E("Video", "threadCreate failed (0x%x)", static_cast<unsigned>(rc));
        delete job;
        return;
    }
    const Result startRc = threadStart(&decodeThread_);
    if (R_FAILED(startRc)) {
        SF_LOG_E("Video", "threadStart failed (0x%x)", static_cast<unsigned>(startRc));
        threadClose(&decodeThread_);
        return;
    }
    decodeThreadActive_ = true;
    SF_LOG_I("Video", "Decode thread started (prio=0x%X stack=%zuKiB)", prio,
             kDecodeStackSize / 1024);
#else
    decodeThread_ = std::thread([this, path, generation] { decodeLoop(path, generation); });
#endif
}

void VideoPlayer::onSelectionChanged(const std::string& videoPath)
{
    stop();
    pendingPath_ = videoPath;
    hoverTimer_ = 0.f;
    waitingHover_ = !videoPath.empty() && enabled_;

    if (videoPath.empty()) {
        SF_LOG_I("Video", "Selection cleared (no video path)");
        return;
    }

    const bool exists = FileSystem::exists(videoPath);
    SF_LOG_I("Video", "Selection: path=%s exists=%s enabled=%s hover=%.1fs",
             videoPath.c_str(), exists ? "yes" : "no", enabled_ ? "yes" : "no",
             hoverDelaySeconds_);
    if (!enabled_)
        SF_LOG_W("Video", "Preview disabled (Applet Mode or settings)");
    else if (!exists)
        SF_LOG_W("Video", "File not found — check <roms>/<system>/videos/<stem>-video.mp4");
}

void VideoPlayer::stop()
{
    stopDecode_ = true;
    ++generation_;
    joinDecodeThread();
    stopDecode_ = false;

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        latestFrame_ = {};
        spareBuffer_ = {};
        hasNewFrame_ = false;
    }
    playing_ = false;
    previewAudio_.stop();
    waitingHover_ = false;
    pendingPath_.clear();
}

void VideoPlayer::shutdown()
{
    enabled_ = false;
    stop();
}

void VideoPlayer::decodeLoop(std::string path, uint64_t generation)
{
    SF_LOG_I("Video", "Decode thread entered (gen=%llu)",
             static_cast<unsigned long long>(generation));
    try {
        runDecode(path, generation);
    } catch (const std::exception& e) {
        SF_LOG_E("Video", "Decode thread exception: %s", e.what());
    } catch (...) {
        SF_LOG_E("Video", "Decode thread exception (unknown)");
    }
    playing_ = false;
    SF_LOG_I("Video", "Decode thread exiting (gen=%llu)",
             static_cast<unsigned long long>(generation));
}

void VideoPlayer::runDecode(const std::string& path, uint64_t generation)
{
#if defined(SF_HAS_FFMPEG)
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    const std::string url = toFFmpegUrl(path);

    AVFormatContext* fmt = avformat_alloc_context();
    if (!fmt) {
        SF_LOG_E("Video", "avformat_alloc_context failed");
        return;
    }

    // Installed before open so a stalled SD read can still be aborted by stop().
    fmt->interrupt_callback.callback = ffmpegInterrupt;
    fmt->interrupt_callback.opaque = this;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "probesize", "4194304", 0);
    av_dict_set(&opts, "analyzeduration", "5000000", 0);
    av_dict_set(&opts, "protocol_whitelist", "file", 0);

    SF_LOG_I("Video", "Opening: %s", url.c_str());
    int err = avformat_open_input(&fmt, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (err < 0) {
        SF_LOG_E("Video", "avformat_open_input failed (%d): %s — %s", err,
                 ffmpegError(err, errbuf), url.c_str());
        return;
    }
    SF_LOG_I("Video", "Container open: %s", fmt->iformat ? fmt->iformat->name : "?");

    err = avformat_find_stream_info(fmt, nullptr);
    if (err < 0) {
        SF_LOG_E("Video", "find_stream_info failed (%d): %s — %s", err,
                 ffmpegError(err, errbuf), path.c_str());
        avformat_close_input(&fmt);
        return;
    }
    SF_LOG_I("Video", "Stream info ready (%u streams)", fmt->nb_streams);

    int streamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        SF_LOG_E("Video", "No video stream in %s", path.c_str());
        avformat_close_input(&fmt);
        return;
    }

    int audioStreamIndex = -1;
    if (audioEnabled_) {
        audioStreamIndex =
            av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, streamIndex, nullptr, 0);
        if (audioStreamIndex >= 0)
            SF_LOG_I("Video", "Audio stream index %d", audioStreamIndex);
    }

    AVStream* stream = fmt->streams[streamIndex];
    stream->discard = AVDISCARD_DEFAULT;

    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        SF_LOG_E("Video", "No decoder for codec_id=%d in %s", stream->codecpar->codec_id,
                 path.c_str());
        avformat_close_input(&fmt);
        return;
    }

    SF_LOG_I("Video", "Decoder: %s (%dx%d) for %s", codec->name, stream->codecpar->width,
             stream->codecpar->height, path.c_str());

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        SF_LOG_E("Video", "avcodec_alloc_context3 failed");
        avformat_close_input(&fmt);
        return;
    }
    avcodec_parameters_to_context(codecCtx, stream->codecpar);
    codecCtx->thread_count = 1;
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        SF_LOG_E("Video", "avcodec_open2 failed for %s (%s)", codec->name, path.c_str());
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return;
    }

    AVCodecContext* audioCtx = nullptr;
    SwrContext* swr = nullptr;
    bool audioReady = false;
    int audioOutRate = PreviewAudio::kSampleRate;
    int audioOutChannels = PreviewAudio::kChannels;

    if (audioStreamIndex >= 0) {
        AVStream* audioStream = fmt->streams[audioStreamIndex];
        const AVCodec* audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (audioCodec) {
            audioCtx = avcodec_alloc_context3(audioCodec);
            if (audioCtx &&
                avcodec_parameters_to_context(audioCtx, audioStream->codecpar) >= 0) {
                audioCtx->thread_count = 1;
                if (avcodec_open2(audioCtx, audioCodec, nullptr) == 0) {
                    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
                    AVChannelLayout inLayout;
                    av_channel_layout_copy(&inLayout, &audioCtx->ch_layout);
                    if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_S16, audioOutRate,
                                            &inLayout, audioCtx->sample_fmt,
                                            audioCtx->sample_rate, 0, nullptr) >= 0 &&
                        swr_init(swr) == 0) {
                        audioReady = true;
                        SF_LOG_I("Video", "Audio decoder: %s %dHz %dch", audioCodec->name,
                                 audioCtx->sample_rate, audioCtx->ch_layout.nb_channels);
                    } else {
                        SF_LOG_W("Video", "swr_init failed — video only");
                        if (swr) {
                            swr_free(&swr);
                            swr = nullptr;
                        }
                    }
                    av_channel_layout_uninit(&inLayout);
                } else {
                    SF_LOG_W("Video", "avcodec_open2 audio failed — video only");
                    avcodec_free_context(&audioCtx);
                    audioCtx = nullptr;
                }
            }
        }
    }

    const int srcW = codecCtx->width;
    const int srcH = codecCtx->height;
    if (srcW <= 0 || srcH <= 0) {
        SF_LOG_E("Video", "Invalid frame size %dx%d", srcW, srcH);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return;
    }

    int w = srcW;
    int h = srcH;
    if (w > kMaxPreviewWidth) {
        w = kMaxPreviewWidth;
        h = (srcH * kMaxPreviewWidth) / srcW;
        h &= ~1;
    }
    if (h > kMaxPreviewHeight) {
        h = kMaxPreviewHeight;
        w = (srcW * kMaxPreviewHeight) / srcH;
        w &= ~1;
    }
    w = std::max(2, w);
    h = std::max(2, h);

    AVFrame* frame = av_frame_alloc();
    AVFrame* rgba = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    if (!frame || !rgba || !pkt) {
        SF_LOG_E("Video", "Decoder setup failed (frame alloc)");
        av_packet_free(&pkt);
        av_frame_free(&rgba);
        av_frame_free(&frame);
        if (swr)
            swr_free(&swr);
        if (audioCtx)
            avcodec_free_context(&audioCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return;
    }

    AVPixelFormat srcPixFmt = resolveVideoPixelFormat(fmt, codecCtx, streamIndex, pkt, frame);
    if (!isValidPixelFormat(srcPixFmt)) {
        SF_LOG_E("Video", "Could not resolve pixel format for %s", path.c_str());
        av_packet_free(&pkt);
        av_frame_free(&rgba);
        av_frame_free(&frame);
        if (swr)
            swr_free(&swr);
        if (audioCtx)
            avcodec_free_context(&audioCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return;
    }

    SwsContext* sws = createScaleContext(srcW, srcH, srcPixFmt, w, h);

    const int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, w, h, 1);
    std::vector<uint8_t> buffer(bufSize > 0 ? static_cast<size_t>(bufSize) : 0);

    if (!sws || buffer.empty()) {
        SF_LOG_E("Video", "Decoder setup failed (sws=%d buf=%d)", sws != nullptr, bufSize);
    } else {
        av_image_fill_arrays(rgba->data, rgba->linesize, buffer.data(), AV_PIX_FMT_RGBA, w, h, 1);

        SF_LOG_I("Video", "Decoding started: %s (%dx%d -> %dx%d)", path.c_str(), srcW, srcH, w, h);
        playing_ = true;

        AudioWorker audioWorker;
        AudioThreadGuard audioGuard;
        audioGuard.worker = &audioWorker;

        if (audioReady) {
            audioWorker.ctx = audioCtx;
            audioWorker.swr = swr;
            audioWorker.outRate = audioOutRate;
            audioWorker.outChannels = audioOutChannels;
            audioWorker.sink = &previewAudio_;

#if defined(__SWITCH__)
            const int audioPrio = audioDecodeThreadPriority();
            if (R_SUCCEEDED(threadCreate(&audioGuard.thread, audioWorkerEntry, &audioWorker,
                                         nullptr, kAudioDecodeStackSize, audioPrio, -2))
                && R_SUCCEEDED(threadStart(&audioGuard.thread))) {
                audioGuard.running = true;
                SF_LOG_I("Video", "Audio decode thread started (prio=0x%X)", audioPrio);
            } else {
                SF_LOG_W("Video", "Audio decode thread failed to start — video only");
            }
#else
            audioGuard.thread = std::thread([&audioWorker] { audioWorker.run(); });
            audioGuard.running = true;
#endif

            if (audioGuard.running)
                previewAudio_.start();
        }

        const bool audioWorkerRunning = audioGuard.running;

        // Frames later than this against the presentation clock are discarded before scaling.
        double frameInterval = 1.0 / 30.0;
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
            frameInterval = av_q2d(av_inv_q(stream->avg_frame_rate));
        frameInterval = std::clamp(frameInterval, 1.0 / 120.0, 1.0 / 10.0);
        const double dropThreshold = frameInterval * 1.5;
        constexpr int kMaxConsecutiveDrops = 4;

        std::vector<uint8_t> scratch;
        PlaybackClock playbackClock;
        bool loggedFirstFrame = false;
        int consecutiveDrops = 0;
        size_t droppedFrames = 0;

        while (!stopDecode_ && generation_ == generation) {
            if (av_read_frame(fmt, pkt) < 0) {
                if (!loopPlayback_)
                    break;
                av_seek_frame(fmt, streamIndex, 0, AVSEEK_FLAG_BACKWARD);
                if (audioStreamIndex >= 0)
                    av_seek_frame(fmt, audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(codecCtx);
                if (audioWorkerRunning)
                    audioWorker.requestFlush();
                previewAudio_.clear();
                playbackClock.reset();
                consecutiveDrops = 0;
                continue;
            }

            if (pkt->stream_index == audioStreamIndex) {
                if (audioWorkerRunning) {
                    if (AVPacket* owned = av_packet_alloc()) {
                        av_packet_move_ref(owned, pkt);
                        audioWorker.submit(owned);
                    }
                }
                av_packet_unref(pkt);
                continue;
            }

            if (pkt->stream_index != streamIndex) {
                av_packet_unref(pkt);
                continue;
            }

            if (avcodec_send_packet(codecCtx, pkt) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    if (stopDecode_ || generation_ != generation)
                        break;

                    double pts = 0.0;
                    if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                        pts = static_cast<double>(frame->best_effort_timestamp)
                              * av_q2d(stream->time_base);

                    // Behind the clock: skip scale + upload so playback catches up instead of
                    // sliding further out of sync.
                    if (consecutiveDrops < kMaxConsecutiveDrops
                        && playbackClock.latenessFor(pts) > dropThreshold) {
                        ++consecutiveDrops;
                        ++droppedFrames;
                        continue;
                    }
                    consecutiveDrops = 0;

                    const auto frameFmt = static_cast<AVPixelFormat>(frame->format);
                    if (!isValidPixelFormat(frameFmt) || frame->width <= 0 || frame->height <= 0)
                        continue;
                    if (frameFmt != srcPixFmt) {
                        srcPixFmt = frameFmt;
                        codecCtx->pix_fmt = frameFmt;
                        sws_freeContext(sws);
                        sws = createScaleContext(codecCtx->width, codecCtx->height, srcPixFmt, w, h);
                        if (!sws)
                            break;
                    }

                    sws_scale(sws, frame->data, frame->linesize, 0, srcH, rgba->data,
                              rgba->linesize);
                    if (scratch.empty())
                        takeSpareBuffer(scratch);
                    packTightRgba(rgba->data[0], rgba->linesize[0], w, h, scratch);

                    playbackClock.waitUntil(pts);

                    {
                        std::lock_guard<std::mutex> lock(frameMutex_);
                        latestFrame_.rgba.swap(scratch);
                        latestFrame_.width = w;
                        latestFrame_.height = h;
                        latestFrame_.pts = pts;
                        hasNewFrame_ = true;
                    }

                    if (!loggedFirstFrame) {
                        SF_LOG_I("Video", "First frame decoded (%dx%d)", w, h);
                        loggedFirstFrame = true;
                    }
                }
            }
            av_packet_unref(pkt);
        }

        if (droppedFrames > 0)
            SF_LOG_D("Video", "Dropped %zu late frames", droppedFrames);

        audioGuard.join();
    }

    SF_LOG_I("Video", "Decode loop ended: %s", path.c_str());

    previewAudio_.stop();
    if (swr)
        swr_free(&swr);
    if (audioCtx)
        avcodec_free_context(&audioCtx);
    if (sws)
        sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&rgba);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);
#else
    SF_LOG_W("Video", "FFmpeg not compiled in — stub preview for %s", path.c_str());
    const int w = 320, h = 180;
    int frameN = 0;
    playing_ = true;
    while (!stopDecode_ && generation_ == generation) {
        VideoFrame vf;
        vf.width = w;
        vf.height = h;
        vf.rgba.resize(static_cast<size_t>(w * h * 4));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                size_t i = static_cast<size_t>((y * w + x) * 4);
                vf.rgba[i] = static_cast<uint8_t>((x + frameN) & 0xFF);
                vf.rgba[i + 1] = static_cast<uint8_t>((y * 2) & 0xFF);
                vf.rgba[i + 2] = static_cast<uint8_t>(80);
                vf.rgba[i + 3] = 255;
            }
        }
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            latestFrame_ = std::move(vf);
            hasNewFrame_ = true;
        }
        ++frameN;
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
#endif
}

void VideoPlayer::tick(float delta)
{
    (void)delta;

    if (!enabled_)
        return;

    if (waitingHover_) {
        hoverTimer_ += delta;
        if (hoverTimer_ >= hoverDelaySeconds_) {
            waitingHover_ = false;
            if (!pendingPath_.empty() && FileSystem::exists(pendingPath_)) {
                SF_LOG_I("Video", "Hover delay elapsed — starting decode: %s", pendingPath_.c_str());
                const uint64_t gen = ++generation_;
                startDecodeThread(pendingPath_, gen);
            } else if (!pendingPath_.empty()) {
                SF_LOG_W("Video", "Preview aborted — file missing: %s", pendingPath_.c_str());
            }
        }
    }
}

bool VideoPlayer::hasFrame() const
{
    std::lock_guard<std::mutex> lock(frameMutex_);
    return !latestFrame_.rgba.empty() && latestFrame_.width > 0 && latestFrame_.height > 0;
}

bool VideoPlayer::tryConsumeFrame(VideoFrame& out)
{
    std::lock_guard<std::mutex> lock(frameMutex_);
    if (!hasNewFrame_ || latestFrame_.rgba.empty())
        return false;
    out = std::move(latestFrame_);
    latestFrame_ = {};
    hasNewFrame_ = false;
    return true;
}

void VideoPlayer::recycleFrame(VideoFrame&& frame)
{
    std::lock_guard<std::mutex> lock(frameMutex_);
    if (spareBuffer_.empty())
        spareBuffer_.swap(frame.rgba);
}

void VideoPlayer::takeSpareBuffer(std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> lock(frameMutex_);
    if (!spareBuffer_.empty())
        out.swap(spareBuffer_);
}

} // namespace sf
