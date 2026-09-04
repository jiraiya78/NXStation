#pragma once

#include "app/Models.hpp"

#include <string>

namespace sf {

enum class GameArtMode {
    BoxArt,
    Thumbnail,
};

/** Path shown in the game list preview (box art or thumbnail per user setting). */
std::string resolveGameListArtPath(const GameMetadata& meta);

} // namespace sf
