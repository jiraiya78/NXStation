#pragma once

#include <string>

namespace sf {

enum class SystemBrowserStyle {
    Carousel,
    List,
};

SystemBrowserStyle systemBrowserStyleFromString(const std::string& value);
std::string systemBrowserStyleToString(SystemBrowserStyle mode);
const char* systemBrowserStyleLabel(SystemBrowserStyle mode);
SystemBrowserStyle nextSystemBrowserStyle(SystemBrowserStyle mode);

} // namespace sf
