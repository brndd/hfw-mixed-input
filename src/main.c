#include <windows.h>
#include "logger.h"
#include "proxy.h"
#include "patches.h"
#include "context_hook.h"

static DWORD WINAPI InitThread(LPVOID lpParam) {
    (void)lpParam;
    
    // Brief pause for early CRT / module load stabilization
    Sleep(200);

    if (!apply_all_patches()) {
        log_error("Patcher failed to apply all signatures; check game version.");
    }

    if (!init_context_hook()) {
        log_error("Failed to initialize Decima context hook.");
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
        if (!proxy_init()) {
            log_error("Failed to initialize version.dll proxy!");
            return FALSE;
        }
        
        HANDLE thread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        if (thread) {
            CloseHandle(thread);
        } else {
            log_error("Failed to create background initialization thread!");
        }
        break;

    case DLL_PROCESS_DETACH:
        log_close();
        break;
    }

    return TRUE;
}
