#include "steam_input.h"
#include "logger.h"
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint64_t InputHandle_t;
typedef uint64_t InputActionSetHandle_t;

typedef void* (*SteamAPI_SteamInput_v006_fn)(void);
typedef int (*SteamAPI_ISteamInput_GetConnectedControllers_fn)(void* self, InputHandle_t* handlesOut);
typedef InputActionSetHandle_t (*SteamAPI_ISteamInput_GetActionSetHandle_fn)(void* self, const char* pszActionSetName);
typedef void (*SteamAPI_ISteamInput_ActivateActionSet_fn)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetHandle);
typedef void (*SteamAPI_ISteamInput_ActivateActionSetLayer_fn)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle);
typedef void (*SteamAPI_ISteamInput_DeactivateActionSetLayer_fn)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle);

static SteamAPI_SteamInput_v006_fn fn_SteamInput = NULL;
static SteamAPI_ISteamInput_GetConnectedControllers_fn fn_GetConnectedControllers = NULL;
static SteamAPI_ISteamInput_GetActionSetHandle_fn fn_GetActionSetHandle = NULL;
static SteamAPI_ISteamInput_ActivateActionSet_fn fn_ActivateActionSet = NULL;
static SteamAPI_ISteamInput_ActivateActionSetLayer_fn fn_ActivateActionSetLayer = NULL;
static SteamAPI_ISteamInput_DeactivateActionSetLayer_fn fn_DeactivateActionSetLayer = NULL;

static void* g_pSteamInput = NULL;
static bool g_siapi_ready = false;
static bool g_in_menu_context = true;
static bool g_initial_sync_done = false;

static char g_ingame_set_name[64] = "InGameControls";
static char g_menu_set_name[64] = "MenuControls";
static char g_weaponwheel_layer_name[64] = "WeaponWheel";

static InputActionSetHandle_t g_hInGame = 0;
static InputActionSetHandle_t g_hMenu = 0;
static InputActionSetHandle_t g_hWeaponWheel = 0;

static void load_config(void) {
    char ini_path[MAX_PATH];
    GetModuleFileNameA(NULL, ini_path, MAX_PATH);
    char* last_slash = strrchr(ini_path, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';
        strcat(ini_path, "hfw_mixed_input.ini");
    } else {
        strcpy(ini_path, "hfw_mixed_input.ini");
    }

    GetPrivateProfileStringA("SteamInput", "ingame_action_set", "InGameControls", g_ingame_set_name, sizeof(g_ingame_set_name), ini_path);
    GetPrivateProfileStringA("SteamInput", "menu_action_set", "MenuControls", g_menu_set_name, sizeof(g_menu_set_name), ini_path);
    GetPrivateProfileStringA("SteamInput", "weapon_wheel_layer", "WeaponWheel", g_weaponwheel_layer_name, sizeof(g_weaponwheel_layer_name), ini_path);

    log_info("Steam Input Config: InGameSet='%s', MenuSet='%s', WeaponWheelLayer='%s'", 
             g_ingame_set_name, g_menu_set_name, g_weaponwheel_layer_name);
}

static InputActionSetHandle_t try_resolve(const char* name) {
    if (!name || !name[0] || !g_pSteamInput || !fn_GetActionSetHandle) return 0;
    InputActionSetHandle_t h = fn_GetActionSetHandle(g_pSteamInput, name);
    if (h) {
        log_info("Resolved Action Set/Layer '%s' -> Handle 0x%llX", name, (unsigned long long)h);
    }
    return h;
}

static DWORD WINAPI StartupSyncThread(LPVOID param) {
    (void)param;
    log_info("Startup controller handshake watcher active...");

    for (int attempt = 0; attempt < 30; ++attempt) {
        Sleep(500);

        if (g_initial_sync_done) break;

        if (g_siapi_ready && g_pSteamInput && g_hMenu && g_in_menu_context) {
            InputHandle_t controllers[16];
            int count = fn_GetConnectedControllers(g_pSteamInput, controllers);
            if (count > 0) {
                for (int i = 0; i < count; ++i) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                    log_info("[SIAPI-Startup] Successfully synced initial MenuControls (0x%llX) to controller %d (attempt %d)", 
                             (unsigned long long)g_hMenu, i, attempt + 1);
                }
                g_initial_sync_done = true;
                break;
            }
        }
    }
    return 0;
}

bool steam_input_init(void) {
    load_config();

    HMODULE hSteamApi = GetModuleHandleA("steam_api64.dll");
    if (!hSteamApi) {
        log_warn("steam_api64.dll is not loaded yet; will retry during context changes.");
        return false;
    }

    fn_SteamInput = (SteamAPI_SteamInput_v006_fn)GetProcAddress(hSteamApi, "SteamAPI_SteamInput_v006");
    fn_GetConnectedControllers = (SteamAPI_ISteamInput_GetConnectedControllers_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetConnectedControllers");
    fn_GetActionSetHandle = (SteamAPI_ISteamInput_GetActionSetHandle_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetActionSetHandle");
    fn_ActivateActionSet = (SteamAPI_ISteamInput_ActivateActionSet_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_ActivateActionSet");
    fn_ActivateActionSetLayer = (SteamAPI_ISteamInput_ActivateActionSetLayer_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_ActivateActionSetLayer");
    fn_DeactivateActionSetLayer = (SteamAPI_ISteamInput_DeactivateActionSetLayer_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_DeactivateActionSetLayer");

    if (!fn_SteamInput || !fn_GetConnectedControllers || !fn_GetActionSetHandle ||
        !fn_ActivateActionSet || !fn_ActivateActionSetLayer || !fn_DeactivateActionSetLayer) {
        log_error("Failed to find all required SteamInput API functions in steam_api64.dll!");
        return false;
    }

    g_pSteamInput = fn_SteamInput();
    if (!g_pSteamInput) {
        log_warn("SteamAPI_SteamInput_v006 returned NULL (SteamInput not yet initialized).");
        return false;
    }

    // Resolve InGame Set
    g_hInGame = try_resolve(g_ingame_set_name);
    if (!g_hInGame) g_hInGame = try_resolve("InGameControls");

    // Resolve Menu Set
    g_hMenu = try_resolve(g_menu_set_name);
    if (!g_hMenu) g_hMenu = try_resolve("MenuControls");
    if (!g_hMenu) g_hMenu = try_resolve("Menu");

    // Resolve Weapon Wheel Layer
    g_hWeaponWheel = try_resolve(g_weaponwheel_layer_name);
    if (!g_hWeaponWheel) g_hWeaponWheel = try_resolve("WeaponWheel");
    if (!g_hWeaponWheel) g_hWeaponWheel = try_resolve("Preset_1000002");

    g_siapi_ready = true;
    log_info("Steam Input SIAPI Integration initialized successfully! (InGameSet: 0x%llX, MenuSet: 0x%llX, WeaponWheelLayer: 0x%llX)",
             (unsigned long long)g_hInGame, (unsigned long long)g_hMenu, (unsigned long long)g_hWeaponWheel);

    // Spawn startup watcher thread
    HANDLE hThread = CreateThread(NULL, 0, StartupSyncThread, NULL, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    }

    return true;
}

static void ensure_siapi_ready(void) {
    if (!g_siapi_ready || !g_pSteamInput || (!g_hMenu && !g_hWeaponWheel)) {
        steam_input_init();
    }
}

void steam_input_on_context_change(const char* context_name, bool enabled) {
    if (!context_name) return;
    ensure_siapi_ready();
    if (!g_siapi_ready || !g_pSteamInput) return;

    InputHandle_t controllers[16];
    int count = fn_GetConnectedControllers(g_pSteamInput, controllers);

    // 1. Weapon Wheel: Action Set Layer Toggle
    if (strcmp(context_name, "WeaponWheel") == 0 && g_hWeaponWheel) {
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                if (enabled) {
                    fn_ActivateActionSetLayer(g_pSteamInput, controllers[i], g_hWeaponWheel);
                    log_info("[SIAPI] Activated Weapon Wheel Layer (0x%llX) on controller %d", 
                             (unsigned long long)g_hWeaponWheel, i);
                } else {
                    fn_DeactivateActionSetLayer(g_pSteamInput, controllers[i], g_hWeaponWheel);
                    log_info("[SIAPI] Deactivated Weapon Wheel Layer (0x%llX) on controller %d", 
                             (unsigned long long)g_hWeaponWheel, i);
                }
            }
        }
        return;
    }

    // 2. Menu: Full Action Set Switch (MenuControls <-> InGameControls)
    // Only check "Menu" (which envelopes all sub-menus and avoids redundant switches)
    if (strcmp(context_name, "Menu") == 0) {
        g_in_menu_context = enabled;
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                if (enabled && g_hMenu) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                    log_info("[SIAPI] Switched to Menu Action Set (0x%llX) on controller %d", 
                             (unsigned long long)g_hMenu, i);
                    g_initial_sync_done = true;
                } else if (!enabled && g_hInGame) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hInGame);
                    log_info("[SIAPI] Restored InGame Action Set (0x%llX) on controller %d", 
                             (unsigned long long)g_hInGame, i);
                    g_initial_sync_done = true;
                }
            }
        }
        return;
    }

    // 3. GamepadActive: Decima just initialized/unlocked the Gamepad subsystem
    if (strcmp(context_name, "GamepadActive") == 0 && enabled) {
        log_info("[SIAPI] GamepadActive enabled (MenuState=%d)", g_in_menu_context);
        if (g_in_menu_context && g_hMenu && count > 0) {
            for (int i = 0; i < count; ++i) {
                fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                log_info("[SIAPI] Re-applied Menu Action Set (0x%llX) on GamepadActive for controller %d", 
                         (unsigned long long)g_hMenu, i);
            }
            g_initial_sync_done = true;
        }
        return;
    }
}
