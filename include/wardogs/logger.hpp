#pragma once

#include <filesystem>
#include <string_view>

namespace wardogs {

enum class LogLevel { debug, info, warning, error };

bool initialize_session_log(const std::filesystem::path& path,
                            std::string_view version) noexcept;
void shutdown_session_log() noexcept;
void write_log(LogLevel level, std::string_view message) noexcept;
[[nodiscard]] std::filesystem::path active_log_path();

inline void log_debug(std::string_view message) noexcept {
    write_log(LogLevel::debug, message);
}
inline void log_info(std::string_view message) noexcept {
    write_log(LogLevel::info, message);
}
inline void log_warning(std::string_view message) noexcept {
    write_log(LogLevel::warning, message);
}
inline void log_error(std::string_view message) noexcept {
    write_log(LogLevel::error, message);
}

}  // namespace wardogs
