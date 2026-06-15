#include "svc/logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::enableFileLog(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path p(filename);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    file_.open(filename, std::ios::out | std::ios::trunc);
    if (file_.is_open()) {
        std::cout << "[Logger] log file: " << filename << std::endl;
    }
}

void Logger::setLevel(LogLevel level) {
    min_level_ = level;
}

void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info(const std::string& msg)  { log(LogLevel::INFO, msg); }
void Logger::warn(const std::string& msg)  { log(LogLevel::WARN, msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERR, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string formatted = "[" + timestamp() + "] [" + levelToString(level) + "] " + msg;

    std::cout << formatted << std::endl;
    if (file_.is_open()) {
        file_ << formatted << std::endl;
        file_.flush();
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR:   return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_tm;
    localtime_s(&local_tm, &time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}
