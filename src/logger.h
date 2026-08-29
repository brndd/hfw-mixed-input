#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

void log_init(void);
void log_debug(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_error(const char* fmt, ...);
void log_close(void);
