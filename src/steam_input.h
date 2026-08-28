#pragma once

#include <stdbool.h>

bool steam_input_init(void);
void steam_input_on_context_change(const char* context_name, bool enabled);
