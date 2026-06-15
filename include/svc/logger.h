#pragma once
// ============================================================================
// Logger - Thread-safe logging with timestamps and severity levels.
// Writes to stdout and, optionally, to a file.
// ============================================================================

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

enum class LogLevel {
    DEBUG,  // internal detail (positions, pixel values)
    INFO,   // normal flow (state changes, actions)
    WARN,   // unexpected but recoverable
    ERR     // errors that prevent operation
};

class Logger {
public:
    static Logger& instance();

    void enableFileLog(const std::string& filename);
    void setLevel(LogLevel level);

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

private:
    Logger() = default;
    void log(LogLevel level, const std::string& msg);
    std::string levelToString(LogLevel level);
    std::string timestamp();

    LogLevel min_level_ = LogLevel::INFO;
    std::ofstream file_;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
