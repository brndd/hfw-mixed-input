#include "config.hpp"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

namespace mod::config {

static std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

void load() {
    std::ifstream file("mixed_input_fix.ini");
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';' || trimmed[0] == '[') {
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        auto key = trim(trimmed.substr(0, eq_pos));
        auto val = trim(trimmed.substr(eq_pos + 1));

        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::tolower(c); });

        if (key == "mode") {
            if (val == "raw_mouse" || val == "raw") {
                g_config.mode = InputMode::RawMouse;
            } else {
                g_config.mode = InputMode::Siapi;
            }
        } else if (key == "disable_mouse_smoothing" || key == "disable_negative_acceleration" || key == "disable_negative_accel") {
            if (val == "true" || val == "1" || val == "yes" || val == "on") {
                g_config.disable_mouse_smoothing = true;
            } else {
                g_config.disable_mouse_smoothing = false;
            }
        } else if (key == "log_level") {
            if (val == "debug") g_config.log_level = 0;
            else if (val == "info") g_config.log_level = 1;
            else if (val == "warn") g_config.log_level = 2;
            else if (val == "error") g_config.log_level = 3;
            else if (val == "off" || val == "none" || val == "false") g_config.log_level = 4;
            else {
                try {
                    g_config.log_level = std::stoi(val);
                } catch (...) {}
            }
        }
    }
}

} // namespace mod::config
