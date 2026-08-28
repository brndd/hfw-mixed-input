#include <windows.h>
#include "logger.h"
#include "proxy.h"
#include "patches.h"

static DWORD WINAPI PatchThread(LPVOID lpParam) {
    (void)lpParam;
    
    // Allow the main executable to finish early CRT/PE loader initialization
    Sleep(500);

    log_info("Starting background patcher thread...");
    
    // Attempt pattern scan and patch
    int max_retries = 10;
    bool success = false;
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        log_info("Patch attempt %d of %d...", attempt, max_retries);
        if (apply_all_patches()) {
            success = true;
            break;
        }
        Sleep(500);
    }

    if (success) {
        log_info("All patches successfully active! Mixed input is ready.");
    } else {
        log_error("Could not apply all patches after %d attempts. Check game version/signatures.", max_retries);
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        log_init();
        log_info("DLL_PROCESS_ATTACH: Loading proxy bindings...");
        if (!proxy_init()) {
            log_error("Failed to initialize version.dll proxy!");
            return FALSE;
        }
        
        HANDLE thread = CreateThread(NULL, 0, PatchThread, NULL, 0, NULL);
        if (thread) {
            CloseHandle(thread);
        } else {
            log_error("Failed to create background patch thread!");
        }
        break;

    case DLL_PROCESS_DETACH:
        log_close();
        break;
    }

    return TRUE;
}
