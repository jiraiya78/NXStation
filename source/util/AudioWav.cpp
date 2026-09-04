#include "util/AudioWav.hpp"

#include <cstdio>
#include <cstring>

namespace sf::audio {

bool loadWavFile(const std::string& path, std::vector<int16_t>* outSamples, int* outChannels)
{
    if (!outSamples)
        return false;

    outSamples->clear();
    if (outChannels)
        *outChannels = 0;

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;

    char riff[4];
    char wave[4];
    if (std::fread(riff, 1, 4, f) != 4 || std::memcmp(riff, "RIFF", 4) != 0) {
        std::fclose(f);
        return false;
    }

    uint32_t riffSize = 0;
    std::fread(&riffSize, sizeof(riffSize), 1, f);
    (void)riffSize;

    if (std::fread(wave, 1, 4, f) != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
        std::fclose(f);
        return false;
    }

    uint16_t channels = 1;
    uint16_t bits = 16;
    std::vector<uint8_t> data;

    while (!std::feof(f)) {
        char id[4];
        if (std::fread(id, 1, 4, f) != 4)
            break;

        uint32_t size = 0;
        if (std::fread(&size, sizeof(size), 1, f) != 1)
            break;

        if (std::memcmp(id, "fmt ", 4) == 0) {
            uint16_t audioFormat = 0;
            std::fread(&audioFormat, sizeof(audioFormat), 1, f);
            std::fread(&channels, sizeof(channels), 1, f);
            std::fseek(f, 4, SEEK_CUR);
            std::fseek(f, 4, SEEK_CUR);
            std::fseek(f, 2, SEEK_CUR);
            std::fread(&bits, sizeof(bits), 1, f);
            if (size > 16)
                std::fseek(f, static_cast<long>(size - 16), SEEK_CUR);
        } else if (std::memcmp(id, "data", 4) == 0) {
            data.resize(size);
            std::fread(data.data(), 1, size, f);
        } else {
            std::fseek(f, static_cast<long>(size), SEEK_CUR);
        }
    }

    std::fclose(f);

    if (data.empty() || bits != 16 || (channels != 1 && channels != 2))
        return false;

    const size_t frameCount = data.size() / (2 * channels);
    outSamples->resize(frameCount * channels);
    std::memcpy(outSamples->data(), data.data(), data.size());
    if (outChannels)
        *outChannels = channels;
    return true;
}

} // namespace sf::audio
