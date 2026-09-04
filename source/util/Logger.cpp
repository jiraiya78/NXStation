#include "util/Logger.hpp"
#include "util/FileSystem.hpp"
#include "util/Paths.hpp"

#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#endif

namespace sf {

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::bootMark(const char* message)
{
#ifdef __SWITCH__
    FileSystem::createDirectories(paths::LOG_DIR);
    FILE* f = std::fopen(paths::BOOT_LOG_PATH, "a");
    if (f) {
        std::fprintf(f, "%s\n", message);
        std::fflush(f);
        std::fclose(f);
    }
#else
    (void)message;
#endif
}

void Logger::trimLogIfNeeded(std::size_t maxBytes, std::size_t keepBytes)
{
    if (keepBytes == 0 || keepBytes >= maxBytes)
        keepBytes = maxBytes / 2;

#ifdef __SWITCH__
    // Close any open handle so we can rewrite the file safely.
    {
        auto& self = instance();
        std::lock_guard<std::mutex> lock(self.mutex_);
        if (self.logFile_) {
            std::fflush(self.logFile_);
            std::fclose(self.logFile_);
            self.logFile_ = nullptr;
        }
    }

    struct stat st {};
    if (stat(paths::LOG_PATH, &st) != 0 || !S_ISREG(st.st_mode))
        return;
    if (static_cast<std::size_t>(st.st_size) <= maxBytes)
        return;

    FILE* in = std::fopen(paths::LOG_PATH, "rb");
    if (!in)
        return;

    if (std::fseek(in, -static_cast<long>(keepBytes), SEEK_END) != 0) {
        std::fclose(in);
        return;
    }

    std::vector<char> buf(keepBytes);
    const std::size_t n = std::fread(buf.data(), 1, keepBytes, in);
    std::fclose(in);
    if (n == 0)
        return;

    // Start at the first full line inside the kept tail.
    std::size_t start = 0;
    while (start < n && buf[start] != '\n')
        ++start;
    if (start < n)
        ++start;

    const std::string tmp = std::string(paths::LOG_PATH) + ".trim";
    FILE* out = std::fopen(tmp.c_str(), "wb");
    if (!out)
        return;
    std::fwrite("--- log trimmed ---\n", 1, 19, out);
    if (start < n)
        std::fwrite(buf.data() + start, 1, n - start, out);
    std::fclose(out);

    FileSystem::removeFile(paths::LOG_PATH);
    FileSystem::renameFile(tmp, paths::LOG_PATH);
#else
    (void)maxBytes;
    (void)keepBytes;
#endif
}

void Logger::openLogFile()
{
#ifdef __SWITCH__
    if (logFile_)
        return;
    FileSystem::ensureAppDirectories();
    logFile_ = std::fopen(paths::LOG_PATH, "a");
    if (logFile_)
        std::setvbuf(logFile_, logBuffer_, _IOFBF, sizeof(logBuffer_));
#endif
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    flushNoLock();
}

void Logger::flushNoLock()
{
    if (logFile_)
        std::fflush(logFile_);
}

void Logger::log(LogLevel level, const char* tag, const char* fmt, ...)
{
    if (level < level_)
        return;

    const char* lvl = "I";
    switch (level) {
        case LogLevel::Debug: lvl = "D"; break;
        case LogLevel::Info:  lvl = "I"; break;
        case LogLevel::Warn:  lvl = "W"; break;
        case LogLevel::Error: lvl = "E"; break;
    }

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Open log file before locking — openLogFile must never call log().
    if (!logFile_)
        openLogFile();

    // Each fflush is a synchronous SD write, so only force one when the line is
    // worth losing frames over; the rest ride the file buffer.
    const bool important = level >= LogLevel::Warn;

    std::lock_guard<std::mutex> lock(mutex_);
    std::printf("[%s][%s] %s\n", lvl, tag ? tag : "SF", buf);
    if (important)
        std::fflush(stdout);

    if (logFile_) {
        std::fprintf(logFile_, "[%s][%s] %s\n", lvl, tag ? tag : "SF", buf);
        if (important)
            std::fflush(logFile_);
    }
}

} // namespace sf
