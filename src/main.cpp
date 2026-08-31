#include <windows.h>
#include <format>
#include "config.hpp"
#include "logger.hpp"
#include "proxy.hpp"
#include "patches.hpp"
#include "camera_hook.hpp"
#include "context_hook.hpp"

#ifndef MOD_VERSION
#define MOD_VERSION "v0.3-custom"
#endif

namespace mod {

static DWORD WINAPI init_thread(LPVOID) {
    // Brief pause for early CRT / module load stabilization
    Sleep(200);

    logger::info("=== Mixed Input Fix {} Initialized ===", MOD_VERSION);

    if (config::g_config.mode == config::InputMode::Siapi) {
        if (!camera::init()) {
            logger::error("Failed to initialize SIAPI camera hook.");
        }
    }

    if (!context::init()) {
        logger::error("Failed to initialize Decima context hook.");
    }

    return 0;
}

} // namespace mod

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        mod::config::load();
        mod::logger::init();
        if (!mod::proxy::init()) {
            mod::logger::error("Failed to initialize version.dll proxy!");
            return FALSE;
        }

        // Apply static memory patches synchronously before game initialization
        if (mod::config::g_config.mode == mod::config::InputMode::RawMouse) {
            mod::logger::info("Active Mode: RAW_MOUSE");
            if (!mod::patches::apply_all_patches()) {
                mod::logger::error("Patcher failed to apply all signatures; check game version.");
            }
        } else {
            mod::logger::info("Active Mode: SIAPI");
        }

        if (mod::config::g_config.disable_mouse_smoothing) {
            mod::logger::info("Mouse Smoothing Patch enabled (engine mouse smoothing disabled)");
            if (!mod::patches::apply_mouse_smoothing_patch()) {
                mod::logger::warn("Failed to apply binary patch for mouse smoothing bypass.");
            }
        } else {
            mod::logger::info("Mouse Smoothing Patch disabled (untouched engine behavior)");
        }

        // Spawn background worker for hooks that depend on runtime module loading (Steam API, Camera, Context)
        if (HANDLE thread = CreateThread(nullptr, 0, mod::init_thread, nullptr, 0, nullptr); thread) {
            CloseHandle(thread);
        } else {
            mod::logger::error("Failed to create background initialization thread!");
        }
        break;

    case DLL_PROCESS_DETACH:
        mod::logger::close();
        break;
    }

    return TRUE;
}
