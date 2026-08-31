#include "logger.hpp"
#include "config.hpp"
#include <fstream>
#include <mutex>
#include <windows.h>

namespace mod::logger {

static std::ofstream g_log_file;
static std::mutex g_log_mutex;

void init(std::string_view filename) {
    if (config::g_config.log_level >= 4) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_log_file.is_open()) {
        g_log_file.open(std::string(filename), std::ios::out | std::ios::trunc);
    }
}

void close() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
}

void log(Level level, std::string_view message) {
    if (static_cast<int>(level) < config::g_config.log_level) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    const char* level_str = "INFO";
    switch (level) {
        case Level::Debug: level_str = "DEBUG"; break;
        case Level::Info:  level_str = "INFO";  break;
        case Level::Warn:  level_str = "WARN";  break;
        case Level::Error: level_str = "ERROR"; break;
    }

    std::string formatted = std::format("[{:02d}:{:02d}:{:02d}.{:03d}] [{}] {}\n",
                                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                                        level_str, message);

    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file << formatted;
        g_log_file.flush();
    }
    OutputDebugStringA(formatted.c_str());
}

} // namespace mod::logger
