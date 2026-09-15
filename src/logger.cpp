#include "wardogs/logger.hpp"

#include <Windows.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace wardogs {
namespace {

struct LoggerState {
    std::mutex mutex;
    std::ofstream stream;
    std::filesystem::path path;
};

LoggerState& state() {
    static LoggerState value;
    return value;
}

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::debug: return "DEBUG";
    case LogLevel::info: return "INFO";
    case LogLevel::warning: return "WARN";
    case LogLevel::error: return "ERROR";
    }
    return "INFO";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &seconds);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream text;
    text << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.'
         << std::setfill('0') << std::setw(3) << milliseconds.count();
    return text.str();
}

void write_locked(LoggerState& logger, LogLevel level,
                  std::string_view message) {
    if (!logger.stream.is_open()) return;
    logger.stream << '[' << timestamp() << "] [" << level_name(level)
                  << "] [tid " << std::this_thread::get_id() << "] "
                  << message << '\n';
    logger.stream.flush();
}

}  // namespace

bool initialize_session_log(const std::filesystem::path& path,
                            std::string_view version) noexcept {
    auto& logger = state();
    try {
        std::scoped_lock lock(logger.mutex);
        logger.stream.close();
        logger.path.clear();
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        logger.stream.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!logger.stream) return false;
        logger.path = path;
        std::ostringstream message;
        message << "session.start version=" << version
                << " pid=" << GetCurrentProcessId();
        write_locked(logger, LogLevel::info, message.str());
        return true;
    } catch (...) {
        return false;
    }
}

void shutdown_session_log() noexcept {
    auto& logger = state();
    try {
        std::scoped_lock lock(logger.mutex);
        write_locked(logger, LogLevel::info, "session.end");
        logger.stream.close();
    } catch (...) {
    }
}

void write_log(LogLevel level, std::string_view message) noexcept {
    auto& logger = state();
    try {
        std::scoped_lock lock(logger.mutex);
        write_locked(logger, level, message);
    } catch (...) {
    }
}

std::filesystem::path active_log_path() {
    auto& logger = state();
    std::scoped_lock lock(logger.mutex);
    return logger.path;
}

}  // namespace wardogs
