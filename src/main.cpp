#include <windows.h>
#include <format>
#include "config.hpp"
#include "logger.hpp"
#include "proxy.hpp"
#include "patches.hpp"
#include "camera_hook.hpp"
#include "context_hook.hpp"
#include "steam_input.hpp"

#ifndef MOD_VERSION
#define MOD_VERSION "v0.3-custom"
#endif

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

        mod::logger::info("=== Mixed Input Fix {} Initialized ===", MOD_VERSION);

        // 1. Mode configuration & hooks
        if (mod::config::g_config.mode == mod::config::InputMode::RawMouse) {
            mod::logger::info("Active Mode: RAW_MOUSE");
            if (!mod::patches::apply_all_patches()) {
                mod::logger::error("Patcher failed to apply all signatures; check game version.");
            }
        } else {
            mod::logger::info("Active Mode: SIAPI");
            if (!mod::camera::init()) {
                mod::logger::error("Failed to initialize SIAPI camera hook.");
            }
        }

        // 2. Steam Input integration (active in both modes for ActionSet switching & manifest handling)
        if (!mod::steam::init()) {
            mod::logger::error("Failed to initialize Steam Input hook.");
        }

        // 3. Mouse smoothing patch
        if (mod::config::g_config.disable_mouse_smoothing) {
            mod::logger::info("Mouse Smoothing Patch enabled (engine mouse smoothing disabled)");
            if (!mod::patches::apply_mouse_smoothing_patch()) {
                mod::logger::warn("Failed to apply binary patch for mouse smoothing bypass.");
            }
        } else {
            mod::logger::info("Mouse Smoothing Patch disabled (untouched engine behavior)");
        }

        // 4. Decima action context hook
        if (!mod::context::init()) {
            mod::logger::error("Failed to initialize Decima context hook.");
        }
        break;

    case DLL_PROCESS_DETACH:
        mod::logger::close();
        break;
    }

    return TRUE;
}
