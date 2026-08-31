#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace mod::config {

enum class InputMode {
    Siapi = 0,
    RawMouse = 1
};

struct Config {
    InputMode mode = InputMode::Siapi;
    int log_level = 1; // 0=DEBUG, 1=INFO (default), 2=WARN, 3=ERROR, 4=OFF
    bool disable_mouse_smoothing = false; // Default: false (patch disabled, untouched engine behavior)
};

inline Config g_config;

void load();

} // namespace mod::config
