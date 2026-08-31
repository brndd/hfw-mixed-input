#include "camera_hook.hpp"
#include "scanner.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "steam_input.hpp"
#include "patches.hpp"
#include <safetyhook.hpp>
#include <windows.h>
#include <cstdint>

namespace mod::camera {

void* volatile g_pNxInputImpl = nullptr;

static safetyhook::InlineHook g_hook_raw_mouse;
static safetyhook::MidHook g_hook_sample_input;
static safetyhook::InlineHook g_hook_freecam;

// ---------------------------------------------------------------------------
// 1. Raw Mouse Ingestion Singleton Capture (0x14008AF20)
// ---------------------------------------------------------------------------
static void hook_process_raw_mouse(void* this_ptr, void* raw_input_data) {
    g_pNxInputImpl = this_ptr;
    if (config::g_config.disable_mouse_smoothing) {
        *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(this_ptr) + 0x569) = 0;
    }
    g_hook_raw_mouse.call<void>(this_ptr, raw_input_data);
}

// ---------------------------------------------------------------------------
// 2. Gameplay Look Hook: SampleInputLookState MidHook (0x14118B5DD)
// ---------------------------------------------------------------------------
static void process_gameplay_sample_input_look(void* camera_component) {
    auto* mouse_buffer = reinterpret_cast<float*>(reinterpret_cast<char*>(camera_component) + 0x140);
    float tx = 0.0f, ty = 0.0f;
    bool touched = steam::get_touchpad_delta(&tx, &ty);

#ifdef VERBOSE_INPUT_LOGGING
    if (mouse_buffer[0] != 0.0f || mouse_buffer[1] != 0.0f || touched) {
        logger::debug("[GAMEPLAY_HOOK] Native Mouse: ({:.2f}, {:.2f}), Touchpad: ({:.2f}, {:.2f})",
                      mouse_buffer[0], mouse_buffer[1], tx, ty);
    }
#endif

    if (touched) {
        mouse_buffer[0] += tx;
        mouse_buffer[1] -= ty;
    }
}

static void hook_sample_input_mid(safetyhook::Context& ctx) {
    void* camera_component = reinterpret_cast<void*>(ctx.rdi);
    process_gameplay_sample_input_look(camera_component);
}

// ---------------------------------------------------------------------------
// 3. Photo Mode FreeCamera Hook: CalculateFreeCameraLookRotation (0x141190230)
// ---------------------------------------------------------------------------
static void process_freecam_sample_input_look(void* camera_component) {
    auto* mouse_buffer = reinterpret_cast<float*>(reinterpret_cast<char*>(camera_component) + 0x140);
    auto* sensitivity = reinterpret_cast<float*>(reinterpret_cast<char*>(camera_component) + 0x180);

    float multiplier = 1.0f;
    if (sensitivity[0] < 0.001f) {
        float fov_scale = *reinterpret_cast<float*>(reinterpret_cast<char*>(camera_component) + 0x114);
        if (fov_scale <= 0.0f) fov_scale = 1.0f;
        multiplier = fov_scale * 40.0f;
    }

    mouse_buffer[2] = 0.0f;
    mouse_buffer[3] = 0.0f;

    bool rmb_held = steam::is_photo_mode_rmb();
    if (!rmb_held && g_pNxInputImpl) {
        uint32_t rmb_state = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(g_pNxInputImpl) + 0x4c8);
        rmb_held = (rmb_state & 0x4) != 0;
    }

    if (!rmb_held) {
        mouse_buffer[0] = 0.0f;
        mouse_buffer[1] = 0.0f;

        float tx = 0.0f, ty = 0.0f;
        if (steam::get_touchpad_delta(&tx, &ty)) {
            mouse_buffer[0] += tx * multiplier;
            mouse_buffer[1] -= ty * multiplier;

#ifdef VERBOSE_INPUT_LOGGING
            logger::debug("[FREECAM_HOOK] Trackpad Move (No RMB): Raw ({:.2f}, {:.2f}) * Mult {:.1f} -> MouseBuf (+0x140): ({:.2f}, {:.2f}, 0, 0), Sens: ({:.6f}, {:.6f})",
                          tx, ty, multiplier, mouse_buffer[0], mouse_buffer[1], sensitivity[0], sensitivity[1]);
#endif
        }
    } else {
        if (mouse_buffer[0] == 0.0f && mouse_buffer[1] == 0.0f && g_pNxInputImpl) {
            float nx = *reinterpret_cast<float*>(reinterpret_cast<char*>(g_pNxInputImpl) + 0x4fc);
            float ny = *reinterpret_cast<float*>(reinterpret_cast<char*>(g_pNxInputImpl) + 0x500);
            if (nx != 0.0f || ny != 0.0f) {
                mouse_buffer[0] = nx * multiplier;
                mouse_buffer[1] = -ny * multiplier;
            }
        }

        float tx = 0.0f, ty = 0.0f;
        if (steam::get_touchpad_delta(&tx, &ty)) {
            mouse_buffer[0] += tx * multiplier;
            mouse_buffer[1] -= ty * multiplier;
        }

#ifdef VERBOSE_INPUT_LOGGING
        static uint32_t s_rmb_log_throttle = 0;
        if (mouse_buffer[0] != 0.0f || mouse_buffer[1] != 0.0f || (s_rmb_log_throttle++ % 30 == 0)) {
            logger::debug("[FREECAM_HOOK] RMB Held: MouseBuf (+0x140): ({:.2f}, {:.2f}, 0, 0), Sens: ({:.6f}, {:.6f})",
                          mouse_buffer[0], mouse_buffer[1], sensitivity[0], sensitivity[1]);
        }
#endif
    }
}

static void hook_calculate_freecam(void* this_ptr, float delta_time) {
    process_freecam_sample_input_look(this_ptr);
    g_hook_freecam.call<void>(this_ptr, delta_time);
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
bool init() {
    HMODULE main_module = GetModuleHandleA(nullptr);
    if (!main_module) return false;

    auto text_region = scanner::get_module_section(main_module, ".text");
    if (!text_region) return false;

    // 1. Hook SampleInputLookState at mouse buffer convergence point (0x14118B5DD)
    const char* sig_sample = "48 8D 05 ?? ?? ?? ?? 48 89 44 24 30 44 38 BF 10 01 00 00";
    uint8_t* target_sample = scanner::scan(*text_region, sig_sample);
    if (target_sample) {
        auto hook_result = safetyhook::MidHook::create(target_sample, hook_sample_input_mid);
        if (hook_result) {
            g_hook_sample_input = std::move(*hook_result);
            logger::debug("Installed SafetyHook (MidHook) at SampleInputLookState (target: {:p})", static_cast<void*>(target_sample));
        } else {
            logger::error("Failed to install SafetyHook (MidHook) at SampleInputLookState (Error: {})", static_cast<int>(hook_result.error().type));
            return false;
        }
    } else {
        logger::error("Failed to find SampleInputLookState convergence signature!");
        return false;
    }

    // 2. Combine Gamepad and Mouse look in CalculateLookRotation (0x1193CE7)
    const char* sig_mouse_branch = "80 BD ?? ?? 00 00 00 48 8D 44 24 ?? C5 F8 57 C0 74 ?? C4 E3 79 21 44 24";
    uint8_t* patch_site_branch = scanner::scan(*text_region, sig_mouse_branch);
    if (patch_site_branch) {
        uint8_t* patch_addr = patch_site_branch + 16;
        if (patches::write_bytes(patch_addr, patches::LOOK_ROTATION_COMBINE_PATCH)) {
            logger::debug("Patched CalculateLookRotation to combine Gamepad and Mouse look deltas");
        } else {
            logger::error("Failed to write combine patch to CalculateLookRotation");
        }
    } else {
        logger::warn("Failed to find CalculateLookRotation mouse look branch signature!");
    }

    // 3. Photo Mode: Hook CalculateFreeCameraLookRotation (0x141190230)
    const char* sig_freecam = "40 53 48 83 EC 70 C5 F8 29 74 24 60 48 8B D9 C5 C8 57 F6";
    uint8_t* target_freecam = scanner::scan(*text_region, sig_freecam);
    if (target_freecam) {
        auto hook_result = safetyhook::InlineHook::create(target_freecam, hook_calculate_freecam);
        if (hook_result) {
            g_hook_freecam = std::move(*hook_result);
            logger::debug("Installed SafetyHook at CalculateFreeCameraLookRotation (target: {:p})", static_cast<void*>(target_freecam));
        } else {
            logger::warn("Failed to install SafetyHook at CalculateFreeCameraLookRotation (Error: {})", static_cast<int>(hook_result.error().type));
        }
    } else {
        logger::warn("Failed to find CalculateFreeCameraLookRotation entry signature!");
    }

    // 4. Capture NxInputImpl singleton from ProcessRawMouseInput (0x14008AF20)
    const char* sig_raw_mouse = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 40 48 8B FA";
    uint8_t* target_raw = scanner::scan(*text_region, sig_raw_mouse);
    if (target_raw) {
        auto hook_result = safetyhook::InlineHook::create(target_raw, hook_process_raw_mouse);
        if (hook_result) {
            g_hook_raw_mouse = std::move(*hook_result);
            logger::debug("Installed SafetyHook at ProcessRawMouseInput (target: {:p})", static_cast<void*>(target_raw));
        } else {
            logger::warn("Failed to install SafetyHook at ProcessRawMouseInput (Error: {})", static_cast<int>(hook_result.error().type));
        }
    } else {
        logger::warn("Failed to find ProcessRawMouseInput signature!");
    }

    return true;
}

} // namespace mod::camera
