#include "steam_input.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "scanner.hpp"
#include <safetyhook.hpp>
#include <windows.h>
#include <array>
#include <cstring>

namespace mod::steam {

using InputHandle_t = uint64_t;
using InputActionSetHandle_t = uint64_t;
using InputAnalogActionHandle_t = uint64_t;

struct InputAnalogActionData_t {
    int eMode;
    float x;
    float y;
    bool bActive;
};

using SteamAPI_SteamInput_v006_fn = void* (*)(void);
using SteamAPI_ISteamInput_GetConnectedControllers_fn = int (*)(void* self, InputHandle_t* handlesOut);
using SteamAPI_ISteamInput_GetActionSetHandle_fn = InputActionSetHandle_t (*)(void* self, const char* pszActionSetName);
using SteamAPI_ISteamInput_ActivateActionSet_fn = void (*)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetHandle);
using SteamAPI_ISteamInput_ActivateActionSetLayer_fn = void (*)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle);
using SteamAPI_ISteamInput_DeactivateActionSetLayer_fn = void (*)(void* self, InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle);
using SteamAPI_ISteamInput_GetAnalogActionHandle_fn = InputAnalogActionHandle_t (*)(void* self, const char* pszActionName);
using SteamAPI_ISteamInput_GetAnalogActionData_fn = InputAnalogActionData_t (*)(void* self, InputHandle_t inputHandle, InputAnalogActionHandle_t analogActionHandle);

using SteamAPI_ISteamInput_Init_fn = bool (*)(void* self, bool bExplicitlyCallRunFrame);
using SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn = bool (*)(void* self, const char* pchAbsoluteOrRelativePath);

static SteamAPI_SteamInput_v006_fn fn_SteamInput = nullptr;
static SteamAPI_ISteamInput_Init_fn fn_Init = nullptr;
static SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn fn_SetInputActionManifestFilePath = nullptr;
static SteamAPI_ISteamInput_GetConnectedControllers_fn fn_GetConnectedControllers = nullptr;
static SteamAPI_ISteamInput_GetActionSetHandle_fn fn_GetActionSetHandle = nullptr;
static SteamAPI_ISteamInput_ActivateActionSet_fn fn_ActivateActionSet = nullptr;
static SteamAPI_ISteamInput_ActivateActionSetLayer_fn fn_ActivateActionSetLayer = nullptr;
static SteamAPI_ISteamInput_DeactivateActionSetLayer_fn fn_DeactivateActionSetLayer = nullptr;
static SteamAPI_ISteamInput_GetAnalogActionHandle_fn fn_GetAnalogActionHandle = nullptr;
static SteamAPI_ISteamInput_GetAnalogActionData_fn fn_GetAnalogActionData = nullptr;

static void* g_pSteamInput = nullptr;
static bool g_siapi_ready = false;

static safetyhook::MidHook g_hook_steam_init;
static safetyhook::MidHook g_hook_steam_post_init;

enum ContextFlags : uint32_t {
    CTX_MENU = 1 << 0,
    CTX_LETTERBOX = 1 << 1,
    CTX_CINEMATIC = 1 << 2,
    CTX_DIALOG = 1 << 3,
    CTX_PHOTO_MODE = 1 << 4,
    CTX_PHOTO_MODE_RMB = 1 << 5,
};
static uint32_t g_menu_context_mask = CTX_MENU;

bool is_photo_mode() {
    return (g_menu_context_mask & CTX_PHOTO_MODE) != 0;
}

bool is_photo_mode_rmb() {
    return (g_menu_context_mask & CTX_PHOTO_MODE_RMB) != 0;
}

static InputActionSetHandle_t g_hInGame = 0;
static InputActionSetHandle_t g_hMenu = 0;
static InputActionSetHandle_t g_hWeaponWheel = 0;
static InputAnalogActionHandle_t g_hSteamTouchPad = 0;

static void resolve_steam_api() {
    if (fn_GetConnectedControllers) return;

    HMODULE hSteamApi = GetModuleHandleA("steam_api64.dll");
    if (!hSteamApi) return;

    fn_SteamInput = reinterpret_cast<SteamAPI_SteamInput_v006_fn>(GetProcAddress(hSteamApi, "SteamAPI_SteamInput_v006"));
    fn_Init = reinterpret_cast<SteamAPI_ISteamInput_Init_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_Init"));
    fn_SetInputActionManifestFilePath = reinterpret_cast<SteamAPI_ISteamInput_SetInputActionManifestFilePath_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_SetInputActionManifestFilePath"));
    fn_GetConnectedControllers = reinterpret_cast<SteamAPI_ISteamInput_GetConnectedControllers_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetConnectedControllers"));
    fn_GetActionSetHandle = reinterpret_cast<SteamAPI_ISteamInput_GetActionSetHandle_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetActionSetHandle"));
    fn_ActivateActionSet = reinterpret_cast<SteamAPI_ISteamInput_ActivateActionSet_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_ActivateActionSet"));
    fn_ActivateActionSetLayer = reinterpret_cast<SteamAPI_ISteamInput_ActivateActionSetLayer_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_ActivateActionSetLayer"));
    fn_DeactivateActionSetLayer = reinterpret_cast<SteamAPI_ISteamInput_DeactivateActionSetLayer_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_DeactivateActionSetLayer"));
    fn_GetAnalogActionHandle = reinterpret_cast<SteamAPI_ISteamInput_GetAnalogActionHandle_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetAnalogActionHandle"));
    fn_GetAnalogActionData = reinterpret_cast<SteamAPI_ISteamInput_GetAnalogActionData_fn>(GetProcAddress(hSteamApi, "SteamAPI_ISteamInput_GetAnalogActionData"));
}

static void apply_manifest_path(void* pSteamInput) {
    if (!pSteamInput) return;
    resolve_steam_api();
    if (fn_SetInputActionManifestFilePath) {
        char manifest_path[MAX_PATH] = {0};
        if (GetCurrentDirectoryA(MAX_PATH, manifest_path)) {
            strcat(manifest_path, "\\steam_input_manifest.vdf");
            bool set_res = fn_SetInputActionManifestFilePath(pSteamInput, manifest_path);
            logger::info("[SIAPI] SetInputActionManifestFilePath(\"{}\") -> {}", manifest_path, set_res);
        } else {
            fn_SetInputActionManifestFilePath(pSteamInput, "steam_input_manifest.vdf");
        }
    }
}

static void resolve_action_handles() {
    if (!g_pSteamInput) return;
    resolve_steam_api();
    if (!fn_GetActionSetHandle) return;

    g_hInGame = fn_GetActionSetHandle(g_pSteamInput, "InGameControls");
    g_hMenu = fn_GetActionSetHandle(g_pSteamInput, "MenuControls");
    g_hWeaponWheel = fn_GetActionSetHandle(g_pSteamInput, "WeaponWheelControls");
    if (fn_GetAnalogActionHandle) {
        g_hSteamTouchPad = fn_GetAnalogActionHandle(g_pSteamInput, "SteamTouchPad");
    }

    if (g_hInGame || g_hMenu || g_hWeaponWheel || g_hSteamTouchPad) {
        g_siapi_ready = true;
        logger::info("Steam Input integration initialized (InGame: {:#x}, Menu: {:#x}, WeaponWheel: {:#x}, SteamTouchPad: {:#x})",
                     g_hInGame, g_hMenu, g_hWeaponWheel, g_hSteamTouchPad);
    }
}

// ---------------------------------------------------------------------------
// Native Decima SteamInput::Init Detours (0x1400895CD)
// ---------------------------------------------------------------------------
static void midhook_decima_steam_input_init(safetyhook::Context& ctx) {
    g_pSteamInput = reinterpret_cast<void*>(ctx.rcx);
    apply_manifest_path(g_pSteamInput);
}

static void midhook_decima_steam_input_post_init(safetyhook::Context& ctx) {
    (void)ctx;
    resolve_action_handles();
}

bool init() {
    HMODULE main_module = GetModuleHandleA(nullptr);
    if (!main_module) return false;

    auto text_region = scanner::get_module_section(main_module, ".text");
    if (!text_region) {
        text_region = scanner::get_module_section(main_module, "");
    }
    if (!text_region) return false;

    // Pattern for Decima's SteamInput::Init in FUN_140089390 (0x1400895CD)
    const char* sig_init = "48 8B 01 B2 01 FF 10 0F B6 D8 C6 05 ?? ?? ?? ?? 01 48 8D 05";
    uint8_t* match = scanner::scan(*text_region, sig_init);
    if (match) {
        g_hook_steam_init = safetyhook::create_mid(match, midhook_decima_steam_input_init);
        g_hook_steam_post_init = safetyhook::create_mid(match + 7, midhook_decima_steam_input_post_init);
        if (g_hook_steam_init && g_hook_steam_post_init) {
            logger::debug("Hooked Decima native SteamInput::Init at {:#x}", reinterpret_cast<uintptr_t>(match));
        } else {
            logger::warn("Failed to install Decima SteamInput::Init hook");
        }
    } else {
        logger::warn("Failed to find Decima SteamInput::Init signature");
    }

    // Fallback in case Decima already initialized before hook setup
    resolve_steam_api();
    if (fn_SteamInput && !g_pSteamInput) {
        g_pSteamInput = fn_SteamInput();
        if (g_pSteamInput) {
            apply_manifest_path(g_pSteamInput);
            resolve_action_handles();
        }
    }

    return true;
}

static void ensure_siapi_ready() {
    if (!g_siapi_ready || !g_pSteamInput) {
        resolve_steam_api();
        if (fn_SteamInput && !g_pSteamInput) {
            g_pSteamInput = fn_SteamInput();
            if (g_pSteamInput) {
                apply_manifest_path(g_pSteamInput);
            }
        }
        resolve_action_handles();
    }
}

bool get_touchpad_delta(float* out_x, float* out_y) {
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;

    ensure_siapi_ready();
    if (!g_siapi_ready || !g_pSteamInput || !g_hSteamTouchPad || !fn_GetAnalogActionData) {
        return false;
    }

    std::array<InputHandle_t, 16> controllers{};
    int count = fn_GetConnectedControllers(g_pSteamInput, controllers.data());
    if (count <= 0) return false;

    bool had_input = false;
    for (int i = 0; i < count; ++i) {
        auto data = fn_GetAnalogActionData(g_pSteamInput, controllers[i], g_hSteamTouchPad);
        if (data.bActive && (data.x != 0.0f || data.y != 0.0f)) {
            if (out_x) *out_x += data.x;
            if (out_y) *out_y += data.y;
            had_input = true;
        }
    }
    return had_input;
}

static bool calculate_in_menu_state(uint32_t mask) {
    bool is_siapi = (config::g_config.mode == config::InputMode::Siapi);
    if (is_siapi) {
        return (mask & (CTX_MENU | CTX_LETTERBOX | CTX_CINEMATIC | CTX_DIALOG)) != 0
               && !(mask & CTX_PHOTO_MODE);
    } else {
        // In legacy (RawMouse) mode, Photo Mode is kept using MenuControls
        return (mask & (CTX_MENU | CTX_LETTERBOX | CTX_CINEMATIC | CTX_DIALOG | CTX_PHOTO_MODE)) != 0;
    }
}

void on_context_change(std::string_view context_name, bool enabled) {
    if (context_name.empty()) return;
    ensure_siapi_ready();
    if (!g_siapi_ready || !g_pSteamInput) return;

    std::array<InputHandle_t, 16> controllers{};
    int count = fn_GetConnectedControllers(g_pSteamInput, controllers.data());

    // 1. Weapon Wheel: Action Set Layer Toggle
    if (context_name == "WeaponWheel" && g_hWeaponWheel) {
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                if (enabled) {
                    fn_ActivateActionSetLayer(g_pSteamInput, controllers[i], g_hWeaponWheel);
                    logger::debug("[SIAPI] Controller {}: Activated WeaponWheelControls layer ({:#x})", i, g_hWeaponWheel);
                } else {
                    fn_DeactivateActionSetLayer(g_pSteamInput, controllers[i], g_hWeaponWheel);
                    logger::debug("[SIAPI] Controller {}: Deactivated WeaponWheelControls layer ({:#x})", i, g_hWeaponWheel);
                }
            }
        }
        return;
    }

    // 2. Menu/Cinematic/Dialogue/Photo Mode: Action Set Switch
    uint32_t flag = 0;
    if (context_name == "Menu") flag = CTX_MENU;
    else if (context_name == "LetterboxedCinematic") flag = CTX_LETTERBOX;
    else if (context_name == "Cinematic") flag = CTX_CINEMATIC;
    else if (context_name == "DialogChoice") flag = CTX_DIALOG;
    else if (context_name == "Photo Mode") flag = CTX_PHOTO_MODE;
    else if (context_name == "Photo Mode Hide Cursor") flag = CTX_PHOTO_MODE_RMB;

    if (flag) {
        bool was_in_menu = calculate_in_menu_state(g_menu_context_mask);
        if (enabled) {
            g_menu_context_mask |= flag;
        } else {
            g_menu_context_mask &= ~flag;
        }
        bool is_in_menu = calculate_in_menu_state(g_menu_context_mask);

        if (was_in_menu != is_in_menu && count > 0) {
            for (int i = 0; i < count; ++i) {
                if (is_in_menu && g_hMenu) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                    logger::debug("[SIAPI] Controller {}: Switched to MenuControls ({:#x}) [trigger: {}, mask: {:#x}]",
                                  i, g_hMenu, context_name, g_menu_context_mask);
                } else if (!is_in_menu && g_hInGame) {
                    fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hInGame);
                    logger::debug("[SIAPI] Controller {}: Restored InGameControls ({:#x}) [trigger: {}, mask: {:#x}]",
                                  i, g_hInGame, context_name, g_menu_context_mask);
                }
            }
        }
        return;
    }

    // 3. GamepadActive: Decima just initialized/unlocked the Gamepad subsystem
    if (context_name == "GamepadActive" && enabled) {
        bool is_in_menu = calculate_in_menu_state(g_menu_context_mask);
        if (is_in_menu && g_hMenu && count > 0) {
            for (int i = 0; i < count; ++i) {
                fn_ActivateActionSet(g_pSteamInput, controllers[i], g_hMenu);
                logger::debug("[SIAPI] Controller {}: Re-applied MenuControls on GamepadActive ({:#x})",
                              i, g_hMenu);
            }
        }
        return;
    }
}

} // namespace mod::steam
