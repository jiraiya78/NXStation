#pragma once

#include <string>

namespace sf {

enum class CarouselTransition {
    Fade,
    Slide,
    Crossfade,
    Zoom,
    None,
};

CarouselTransition carouselTransitionFromString(const std::string& value);
std::string carouselTransitionToString(CarouselTransition mode);
const char* carouselTransitionLabel(CarouselTransition mode);
CarouselTransition nextCarouselTransition(CarouselTransition mode);

} // namespace sf
