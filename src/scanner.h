#pragma once

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool get_module_section(HMODULE module, const char* section_name, uint8_t** out_base, size_t* out_size);
uint8_t* scan_pattern(uint8_t* base, size_t size, const char* ida_pattern);
