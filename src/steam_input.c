#include "steam_input.h"
#include "logger.h"
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
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

enum {
    CTX_MENU = 1 << 0,
    CTX_LETTERBOX = 1 << 1,
    CTX_CINEMATIC = 1 << 2,
    CTX_DIALOG = 1 << 3,
};
static uint32_t g_menu_context_mask = CTX_MENU;

static InputActionSetHandle_t g_hInGame = 0;
static InputActionSetHandle_t g_hMenu = 0;
static InputActionSetHandle_t g_hWeaponWheel = 0;

bool steam_input_init(void) {
    HMODULE hSteamApi = GetModuleHandleA("steam_api64.dll");
    if (!hSteamApi) {
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
        log_error("Failed to resolve SteamInput API functions in steam_api64.dll.");
        return false;
    }

    g_pSteamInput = fn_SteamInput();
    if (!g_pSteamInput) {
        return false;
    }

    // Resolve exact canonical action set and layer handles
    g_hInGame = fn_GetActionSetHandle(g_pSteamInput, "InGameControls");
    g_hMenu = fn_GetActionSetHandle(g_pSteamInput, "MenuControls");
    g_hWeaponWheel = fn_GetActionSetHandle(g_pSteamInput, "WeaponWheelControls");

    g_siapi_ready = true;
    log_info("Steam Input integration initialized (InGameControls, MenuControls, WeaponWheelControls)");
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
                    log_debug("[SIAPI] Controller %d: Activated WeaponWheelControls layer (0x%llX)", i, (unsigned long long)g_hWeaponWheel);
                } else {
                    fn_DeactivateActionSetLayer(g_pSteamInput, controllers[i], g_hWeaponWheel);
                    log_debug("[SIAPI] Controller %d: Deactivated WeaponWheelControls layer (0x%llX)", i, (unsigned long long)g_hWeaponWheel);
                }
            }
        }
        return;
    }

    // 2. Menu/Cinematic/Dialogue: Full Action Set Switch (MenuControls <-> InGameControls)
    uint32_t flag = 0;
    if (strcmp(context_name, "Menu") == 0) flag = CTX_MENU;
    else if (strcmp(context_name, "LetterboxedCinematic") == 0) flag = CTX_LETTERBOX;
    else if (strcmp(context_name, "Cinematic") == 0) flag = CTX_CINEMATIC;
    else if (strcmp(context_name, "DialogChoice") == 0) flag = CTX_DIALOG;

    if (flag) {
        bool was_in_menu = (g_menu_context_mask != 0);
        if (enabled) {
            g_menu_context_mask |= flag;
        } else {
            g_menu_context_mask &= ~flag;
        }
        bool is_in_menu = (g_menu_context_mask != 0);

        if (was_in_menu != is_in_menu && count > 0) {
            for (int i = 0; i < count; ++i) {
                if (is_in_menu && g_hMenu) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                    log_debug("[SIAPI] Controller %d: Switched to MenuControls (0x%llX) [trigger: %s, mask: 0x%X]", 
                              i, (unsigned long long)g_hMenu, context_name, g_menu_context_mask);
                } else if (!is_in_menu && g_hInGame) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hInGame);
                    log_debug("[SIAPI] Controller %d: Restored InGameControls (0x%llX) [trigger: %s]", 
                              i, (unsigned long long)g_hInGame, context_name);
                }
            }
        }
        return;
    }

    // 3. GamepadActive: Decima just initialized/unlocked the Gamepad subsystem
    if (strcmp(context_name, "GamepadActive") == 0 && enabled) {
        if (g_menu_context_mask != 0 && g_hMenu && count > 0) {
            for (int i = 0; i < count; ++i) {
                fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                log_debug("[SIAPI] Controller %d: Re-applied MenuControls on GamepadActive (0x%llX)", 
                          i, (unsigned long long)g_hMenu);
            }
        }
        return;
    }
}
