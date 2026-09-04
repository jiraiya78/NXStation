#include "util/CloudLog.hpp"
#include "util/FileSystem.hpp"
#include "util/Paths.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace sf {

namespace {

std::mutex gMutex;
FILE* gFile = nullptr;

std::string timestamp()
{
    const time_t now = time(nullptr);
    struct tm tm {};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

} // namespace

void CloudLog::open()
{
    std::lock_guard<std::mutex> lock(gMutex);
    if (gFile)
        return;
    FileSystem::createDirectories(paths::LOG_DIR);
    gFile = std::fopen(paths::CLOUD_LOG_PATH, "a");
}

void CloudLog::close()
{
    std::lock_guard<std::mutex> lock(gMutex);
    if (gFile) {
        std::fclose(gFile);
        gFile = nullptr;
    }
}

void CloudLog::write(const std::string& line)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gFile)
        return;
    std::fprintf(gFile, "[%s] %s\n", timestamp().c_str(), line.c_str());
    std::fflush(gFile);
}

void CloudLog::writef(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(buf);
}

} // namespace sf
