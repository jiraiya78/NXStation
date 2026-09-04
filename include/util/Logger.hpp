#pragma once

#include <cstdio>
#include <cstddef>
#include <mutex>

namespace sf {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level) { level_ = level; }
    void log(LogLevel level, const char* tag, const char* fmt, ...);

    /** Push buffered Info/Debug lines to the SD card. */
    void flush();

    /** flush() without taking the mutex; for the exception handler only. */
    void flushNoLock();

    /** Write immediately to boot.log (before full logger init). */
    static void bootMark(const char* message);

    /**
     * If NXStation.log exceeds maxBytes, keep only the last keepBytes.
     * Call once at startup before heavy logging — cheap when under the limit
     * (one stat); rewrite only when oversized.
     */
    static void trimLogIfNeeded(std::size_t maxBytes = 512 * 1024,
                                std::size_t keepBytes = 256 * 1024);

private:
    Logger() = default;
    void openLogFile();

    LogLevel level_ = LogLevel::Info;
    std::mutex mutex_;
    FILE* logFile_ = nullptr;
    char logBuffer_[32 * 1024]{};
};

} // namespace sf

#define SF_LOG_D(tag, ...) ::sf::Logger::instance().log(::sf::LogLevel::Debug, tag, __VA_ARGS__)
#define SF_LOG_I(tag, ...) ::sf::Logger::instance().log(::sf::LogLevel::Info,  tag, __VA_ARGS__)
#define SF_LOG_W(tag, ...) ::sf::Logger::instance().log(::sf::LogLevel::Warn,  tag, __VA_ARGS__)
#define SF_LOG_E(tag, ...) ::sf::Logger::instance().log(::sf::LogLevel::Error, tag, __VA_ARGS__)
