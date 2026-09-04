#include "util/GameArt.hpp"

#include "app/Config.hpp"
#include "util/FileSystem.hpp"

namespace sf {

std::string resolveGameListArtPath(const GameMetadata& meta)
{
    const bool wantThumb = Config::instance().gameArtMode() == GameArtMode::Thumbnail;

    auto pick = [](const std::string& path) {
        return !path.empty() && FileSystem::exists(path);
    };

    if (wantThumb) {
        if (pick(meta.logoPath))
            return meta.logoPath;
        if (pick(meta.boxArtPath))
            return meta.boxArtPath;
    } else {
        if (pick(meta.boxArtPath))
            return meta.boxArtPath;
        if (pick(meta.logoPath))
            return meta.logoPath;
    }
    return {};
}

} // namespace sf
