#include "media/PreviewAudio.hpp"
#include "media/SwitchAudout.hpp"

#include <algorithm>

namespace sf {

void PreviewAudio::setVolume(float volume)
{
    volume_ = std::clamp(volume, 0.f, 1.f);
}

void PreviewAudio::start()
{
#if defined(__SWITCH__)
    if (!enabled_ || !audio::SwitchAudout::ready())
        return;
    audio::SwitchAudout::startStream();
#endif
    active_ = enabled_;
}

void PreviewAudio::stop()
{
    active_ = false;
#if defined(__SWITCH__)
    audio::SwitchAudout::stopStream();
#endif
}

void PreviewAudio::clear()
{
#if defined(__SWITCH__)
    audio::SwitchAudout::clearStream();
#endif
}

void PreviewAudio::pushPcm(const int16_t* samples, size_t frameCount)
{
    if (!active_ || !enabled_ || frameCount == 0 || !samples)
        return;
    audio::SwitchAudout::pushStream(samples, frameCount, volume_);
}

size_t PreviewAudio::queuedFrames() const
{
    return audio::SwitchAudout::streamQueuedFrames();
}

} // namespace sf
