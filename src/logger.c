#include "logger.h"

#ifndef MOD_VERSION
#define MOD_VERSION "dev"
#endif

static FILE* g_log_file = NULL;
static CRITICAL_SECTION g_log_lock;
static bool g_log_initialized = false;
static bool g_debug_enabled = false;
static bool g_debug_checked = false;

bool log_is_debug_enabled(void) {
    if (!g_debug_checked) {
        char val[8] = {0};
        DWORD len = GetEnvironmentVariableA("INPUTFIX_DEBUG", val, sizeof(val));
        g_debug_enabled = (len > 0 && len < sizeof(val) && val[0] != '0');
        g_debug_checked = true;
    }
    return g_debug_enabled;
}

void log_init(void) {
    if (g_log_initialized) return;
    InitializeCriticalSection(&g_log_lock);
    
    char log_path[MAX_PATH];
    GetModuleFileNameA(NULL, log_path, MAX_PATH);
    char* last_slash = strrchr(log_path, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';
        strcat(log_path, "mixed_input_fix.log");
    } else {
        strcpy(log_path, "mixed_input_fix.log");
    }

    g_log_file = fopen(log_path, "w");
    g_log_initialized = true;
    
    log_info("=== Mixed Input Fix %s Initialized ===", MOD_VERSION);
}

static void log_write(const char* level, const char* fmt, va_list args) {
    if (!g_log_initialized) {
        log_init();
    }
    
    EnterCriticalSection(&g_log_lock);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    if (g_log_file) {
        fprintf(g_log_file, "[%02d:%02d:%02d.%03d] [%s] ", 
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
        vfprintf(g_log_file, fmt, args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
    
    LeaveCriticalSection(&g_log_lock);
}

void log_debug(const char* fmt, ...) {
    if (!log_is_debug_enabled()) return;
    va_list args;
    va_start(args, fmt);
    log_write("DEBUG", fmt, args);
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write("INFO", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write("WARN", fmt, args);
    va_end(args);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write("ERROR", fmt, args);
    va_end(args);
}

void log_close(void) {
    if (!g_log_initialized) return;
    EnterCriticalSection(&g_log_lock);
    if (g_log_file) {
        log_info("=== Shutting Down Mixed Input Fix ===");
        fclose(g_log_file);
        g_log_file = NULL;
    }
    LeaveCriticalSection(&g_log_lock);
    DeleteCriticalSection(&g_log_lock);
    g_log_initialized = false;
}
