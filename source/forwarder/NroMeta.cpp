#include "forwarder/NroMeta.hpp"

#include <cstring>
#include <cstdint>
#include <fstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sf {

namespace {

#ifdef __SWITCH__
constexpr u32 kNroHeaderMagic = NROHEADER_MAGIC;
constexpr u32 kNroAssetMagic = NROASSETHEADER_MAGIC;
#else
constexpr u32 kNroHeaderMagic = 0x304F524E;
constexpr u32 kNroAssetMagic = 0x30453441;
#endif

bool readAt(std::ifstream& in, std::uint64_t offset, void* out, std::size_t size)
{
    in.seekg(static_cast<std::streamoff>(offset));
    if (!in)
        return false;
    in.read(static_cast<char*>(out), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(in.gcount()) == size;
}

} // namespace

Result readNroNacp(const std::string& path, NacpStruct& nacp)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return MAKERESULT(2, 1);

    NroStart start{};
    NroHeader header{};
    if (!readAt(in, 0, &start, sizeof(start)) || !readAt(in, sizeof(start), &header, sizeof(header)))
        return MAKERESULT(2, 2);
    if (header.magic != kNroHeaderMagic)
        return MAKERESULT(2, 3);

    NroAssetHeader asset{};
    if (!readAt(in, header.size, &asset, sizeof(asset)))
        return MAKERESULT(2, 4);
    if (asset.magic != kNroAssetMagic || asset.nacp.size != sizeof(NacpStruct))
        return MAKERESULT(2, 5);

    const std::uint64_t nacpOffset = header.size + asset.nacp.offset;
    if (!readAt(in, nacpOffset, &nacp, sizeof(nacp)))
        return MAKERESULT(2, 6);

    return 0;
}

std::vector<u8> readNroIcon(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};

    NroStart start{};
    NroHeader header{};
    if (!readAt(in, 0, &start, sizeof(start)) || !readAt(in, sizeof(start), &header, sizeof(header))
        || header.magic != kNroHeaderMagic)
        return {};

    NroAssetHeader asset{};
    if (!readAt(in, header.size, &asset, sizeof(asset))
        || asset.magic != kNroAssetMagic || asset.icon.size == 0 || asset.icon.size > 1024 * 1024)
        return {};

    std::vector<u8> icon(static_cast<std::size_t>(asset.icon.size));
    const std::uint64_t iconOffset = header.size + asset.icon.offset;
    if (!readAt(in, iconOffset, icon.data(), icon.size()))
        return {};

    return icon;
}

std::string nroArgvPath(const std::string& path)
{
    std::string normalized = path;
    if (normalized.rfind("sdmc:", 0) != 0)
        normalized = "sdmc:" + normalized;
    if (normalized.find_first_of(" \t(\"')") != std::string::npos)
        return "\"" + normalized + "\"";
    return normalized;
}

} // namespace sf
