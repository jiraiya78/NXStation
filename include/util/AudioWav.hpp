#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sf::audio {

/** Load a 16-bit PCM WAV file into interleaved samples (mono or stereo). */
bool loadWavFile(const std::string& path, std::vector<int16_t>* outSamples, int* outChannels);

} // namespace sf::audio
