#pragma once

#include <string>

namespace sf::audio {

/** Loops theme background music (MP3/OGG/WAV) through the shared Switch audout mixer. */
class ThemeBgmPlayer {
public:
    static ThemeBgmPlayer& instance();

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void setVolume(float volume);
    float volume() const { return volume_; }

    /** Stop any current track and start looping path (empty path stops playback). */
    void reload(const std::string& path);

    void stop();
    void shutdown();

private:
    ThemeBgmPlayer() = default;

    bool enabled_ = true;
    float volume_ = 0.35f;
};

} // namespace sf::audio
