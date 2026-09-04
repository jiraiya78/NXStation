#include "ui/UiTransition.hpp"

namespace sf {

CarouselTransition carouselTransitionFromString(const std::string& value)
{
    if (value == "slide")
        return CarouselTransition::Slide;
    if (value == "crossfade")
        return CarouselTransition::Crossfade;
    if (value == "zoom")
        return CarouselTransition::Zoom;
    if (value == "none")
        return CarouselTransition::None;
    return CarouselTransition::Fade;
}

std::string carouselTransitionToString(CarouselTransition mode)
{
    switch (mode) {
    case CarouselTransition::Slide:
        return "slide";
    case CarouselTransition::Crossfade:
        return "crossfade";
    case CarouselTransition::Zoom:
        return "zoom";
    case CarouselTransition::None:
        return "none";
    case CarouselTransition::Fade:
    default:
        return "fade";
    }
}

const char* carouselTransitionLabel(CarouselTransition mode)
{
    switch (mode) {
    case CarouselTransition::Slide:
        return "Slide";
    case CarouselTransition::Crossfade:
        return "Crossfade";
    case CarouselTransition::Zoom:
        return "Zoom";
    case CarouselTransition::None:
        return "None";
    case CarouselTransition::Fade:
    default:
        return "Fade";
    }
}

CarouselTransition nextCarouselTransition(CarouselTransition mode)
{
    switch (mode) {
    case CarouselTransition::Fade:
        return CarouselTransition::Slide;
    case CarouselTransition::Slide:
        return CarouselTransition::Crossfade;
    case CarouselTransition::Crossfade:
        return CarouselTransition::Zoom;
    case CarouselTransition::Zoom:
        return CarouselTransition::None;
    case CarouselTransition::None:
    default:
        return CarouselTransition::Fade;
    }
}

} // namespace sf
