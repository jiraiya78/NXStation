#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sf {

struct SystemConfig {
    std::string id;
    std::string name;
    std::string path;
    std::string core;
    std::vector<std::string> extensions;
    /** ScreenScraper API systemeid (0 = scraping disabled for this system). */
    int ssSystemId = 0;
};

struct GameMetadata {
    std::string romPath;
    std::string systemId;
    std::string title;
    std::string description;
    std::string developer;
    std::string publisher;
    std::string releaseDate;
    std::string genre;
    std::string boxArtPath;
    std::string logoPath;
    std::string videoPath;
    std::string manualPath;
    std::string crc32;
    bool scraped = false;
};

struct GameItem {
    std::string path;
    std::string filename;
    std::string displayName;
    std::string systemId;
    GameMetadata meta;
    /** Unix epoch seconds when last played; 0 if unknown. Used by Last Played list. */
    uint64_t lastPlayedAt = 0;
    /** Row from the RomM virtual library (RomM server). Empty path = not downloaded yet. */
    bool fromRomM = false;
    std::string rommRomId;
    /** Cloud-available and not yet downloaded locally — shows the cloud badge, A downloads. */
    bool cloudAvailable = false;
    /** Number of files in the RomM set (multi-disc ROMs); 1 for single-file ROMs. */
    int discCount = 1;
};

} // namespace sf
