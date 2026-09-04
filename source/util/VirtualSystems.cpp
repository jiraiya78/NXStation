#include "util/VirtualSystems.hpp"

#include "util/Paths.hpp"

namespace sf {

bool isVirtualSystemId(const std::string& systemId)
{
    return systemId == kFavoritesSystemId || systemId == kLastPlayedSystemId;
}

const char* virtualSystemDisplayName(const std::string& systemId)
{
    if (systemId == kFavoritesSystemId)
        return "Favorites";
    if (systemId == kLastPlayedSystemId)
        return "Last Played";
    return systemId.c_str();
}

const char* virtualSystemBackgroundStem(const std::string& systemId)
{
    if (systemId == kFavoritesSystemId)
        return "favorite";
    if (systemId == kLastPlayedSystemId)
        return "lastplayed";
    return systemId.c_str();
}

std::string virtualSectionRoot(const std::string& systemId)
{
    if (systemId == kFavoritesSystemId)
        return paths::FAVORITES_SECTION_DIR;
    if (systemId == kLastPlayedSystemId)
        return paths::LAST_PLAYED_SECTION_DIR;
    return {};
}

std::string systemAcronym(const std::string& systemId)
{
    if (systemId == "nes")
        return "NES";
    if (systemId == "snes")
        return "SNES";
    if (systemId == "n64")
        return "N64";
    if (systemId == "gba")
        return "GBA";
    if (systemId == "gb")
        return "GB";
    if (systemId == "gbc")
        return "GBC";
    if (systemId == "nds")
        return "NDS";
    if (systemId == "3ds")
        return "3DS";
    if (systemId == "megadrive" || systemId == "genesis")
        return "MEGADRIVE";
    if (systemId == "mastersystem")
        return "SMS";
    if (systemId == "gamegear")
        return "GG";
    if (systemId == "atari2600")
        return "A2600";
    if (systemId == "atari5200")
        return "A5200";
    if (systemId == "atari7800")
        return "A7800";
    if (systemId == "atarilynx")
        return "LYNX";
    if (systemId == "atarijaguar")
        return "JAGUAR";
    if (systemId == "atarist")
        return "ATARIST";
    if (systemId == "gc")
        return "GC";
    if (systemId == "wii")
        return "WII";
    if (systemId == "saturn")
        return "SATURN";
    if (systemId == "arcade")
        return "ARCADE";
    if (systemId == "cps1")
        return "CPS1";
    if (systemId == "cps2")
        return "CPS2";
    if (systemId == "neogeo")
        return "NEOGEO";
    if (systemId == "psx")
        return "PSX";
    if (systemId == "psp")
        return "PSP";
    if (systemId == "dreamcast")
        return "DC";
    if (systemId == "pce")
        return "PCE";
    if (systemId.empty())
        return {};
    // Fallback: uppercase the id.
    std::string out = systemId;
    for (char& c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

} // namespace sf
