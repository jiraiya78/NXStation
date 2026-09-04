#pragma once

#include <string>

namespace sf {

/** Append-only scrape session log at sdmc:/switch/NXStation/log/scrape.log */
class ScrapeLog {
public:
    static void open();
    static void close();
    static void write(const std::string& line);
    static void writef(const char* fmt, ...);
};

} // namespace sf
