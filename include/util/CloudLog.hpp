#pragma once

#include <string>

namespace sf {

/** Append-only cloud backup/restore log at sdmc:/switch/NXStation/log/cloud.log */
class CloudLog {
public:
    static void open();
    static void close();
    static void write(const std::string& line);
    static void writef(const char* fmt, ...);
};

} // namespace sf
