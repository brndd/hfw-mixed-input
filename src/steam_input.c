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
typedef bool (*SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn)(void* self, const char* pchInputActionManifestAbsolutePath);

static SteamAPI_SteamInput_v006_fn fn_SteamInput = NULL;
static SteamAPI_ISteamInput_GetConnectedControllers_fn fn_GetConnectedControllers = NULL;
static SteamAPI_ISteamInput_GetActionSetHandle_fn fn_GetActionSetHandle = NULL;
static SteamAPI_ISteamInput_ActivateActionSet_fn fn_ActivateActionSet = NULL;
static SteamAPI_ISteamInput_ActivateActionSetLayer_fn fn_ActivateActionSetLayer = NULL;
static SteamAPI_ISteamInput_DeactivateActionSetLayer_fn fn_DeactivateActionSetLayer = NULL;
static SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn fn_SetInputActionManifestFilePath = NULL;

static void* g_pSteamInput = NULL;
static bool g_siapi_ready = false;

static char g_ingame_set_name[64] = "MainControls";
static char g_menu_set_name[64] = "MenuControls";
static char g_weaponwheel_layer_name[64] = "WeaponWheelControls";

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

    GetPrivateProfileStringA("SteamInput", "ingame_action_set", "MainControls", g_ingame_set_name, sizeof(g_ingame_set_name), ini_path);
    GetPrivateProfileStringA("SteamInput", "menu_action_set", "MenuControls", g_menu_set_name, sizeof(g_menu_set_name), ini_path);
    GetPrivateProfileStringA("SteamInput", "weapon_wheel_layer", "WeaponWheelControls", g_weaponwheel_layer_name, sizeof(g_weaponwheel_layer_name), ini_path);

    log_info("Steam Input Config: InGame='%s', Menu='%s', WeaponWheel='%s'", 
             g_ingame_set_name, g_menu_set_name, g_weaponwheel_layer_name);
}

static InputActionSetHandle_t resolve_action_set(const char* primary_name, const char* fallback_name) {
    if (!g_pSteamInput || !fn_GetActionSetHandle) return 0;
    
    InputActionSetHandle_t handle = 0;
    if (primary_name && primary_name[0]) {
        handle = fn_GetActionSetHandle(g_pSteamInput, primary_name);
        if (handle) {
            log_info("Resolved Action Set '%s' -> Handle 0x%llX", primary_name, (unsigned long long)handle);
            return handle;
        }
    }
    if (fallback_name && fallback_name[0]) {
        handle = fn_GetActionSetHandle(g_pSteamInput, fallback_name);
        if (handle) {
            log_info("Resolved Action Set fallback '%s' -> Handle 0x%llX", fallback_name, (unsigned long long)handle);
            return handle;
        }
    }
    log_warn("Could not resolve Action Set handle for '%s' (or fallback '%s')", primary_name, fallback_name ? fallback_name : "");
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
    fn_SetInputActionManifestFilePath = (SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn)GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_SetInputActionManifestFilePath");

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

    // Register root steam_input_manifest.vdf if present
    if (fn_SetInputActionManifestFilePath) {
        char manifest_path[MAX_PATH];
        GetModuleFileNameA(NULL, manifest_path, MAX_PATH);
        char* last_slash = strrchr(manifest_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = '\0';
            strcat(manifest_path, "steam_input_manifest.vdf");
        } else {
            strcpy(manifest_path, "steam_input_manifest.vdf");
        }

        DWORD attrib = GetFileAttributesA(manifest_path);
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            bool res = fn_SetInputActionManifestFilePath(g_pSteamInput, manifest_path);
            log_info("Registered root Steam Input Manifest '%s' (Result: %d)", manifest_path, res);
        }
    }

    // Resolve Action Sets with sensible fallbacks
    g_hInGame = resolve_action_set(g_ingame_set_name, "InGameControls");
    g_hMenu = resolve_action_set(g_menu_set_name, "Menu");
    g_hWeaponWheel = resolve_action_set(g_weaponwheel_layer_name, "WeaponWheel");

    g_siapi_ready = true;
    log_info("Steam Input SIAPI Integration initialized successfully!");
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
    if (count <= 0) return;

    // 1. Weapon Wheel Layer Toggle
    if (strcmp(context_name, "WeaponWheel") == 0 && g_hWeaponWheel) {
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
        return;
    }

    // 2. Menu Set Switch (OpaqueMenu / Menu)
    if ((strcmp(context_name, "OpaqueMenu") == 0 || strcmp(context_name, "Menu") == 0) && g_hMenu) {
        for (int i = 0; i < count; ++i) {
            if (enabled) {
                fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                log_info("[SIAPI] Activated Menu Action Set (0x%llX) on controller %d", 
                         (unsigned long long)g_hMenu, i);
            } else if (g_hInGame) {
                fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hInGame);
                log_info("[SIAPI] Restored InGame Action Set (0x%llX) on controller %d", 
                         (unsigned long long)g_hInGame, i);
            }
        }
        return;
    }
}
