#pragma once
#include <cstdint>
#include <string_view>

namespace mod::steam {

bool init();
bool is_photo_mode();
bool is_photo_mode_rmb();
bool get_touchpad_delta(float* out_x, float* out_y);
void on_context_change(std::string_view context_name, bool enabled);

} // namespace mod::steam
