#include "ui/SystemBrowserStyle.hpp"

namespace sf {

SystemBrowserStyle systemBrowserStyleFromString(const std::string& value)
{
    if (value == "list")
        return SystemBrowserStyle::List;
    return SystemBrowserStyle::Carousel;
}

std::string systemBrowserStyleToString(SystemBrowserStyle mode)
{
    switch (mode) {
    case SystemBrowserStyle::List:
        return "list";
    case SystemBrowserStyle::Carousel:
    default:
        return "carousel";
    }
}

const char* systemBrowserStyleLabel(SystemBrowserStyle mode)
{
    switch (mode) {
    case SystemBrowserStyle::List:
        return "List";
    case SystemBrowserStyle::Carousel:
    default:
        return "Carousel";
    }
}

SystemBrowserStyle nextSystemBrowserStyle(SystemBrowserStyle mode)
{
    return mode == SystemBrowserStyle::Carousel ? SystemBrowserStyle::List
                                              : SystemBrowserStyle::Carousel;
}

} // namespace sf
