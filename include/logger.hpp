#pragma once
#include <string_view>
#include <format>
#include <cstdint>

namespace mod::logger {

enum class Level : int {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Off = 4
};

void init(std::string_view filename = "mixed_input_fix.log");
void close();
void log(Level level, std::string_view message);

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace mod::logger
