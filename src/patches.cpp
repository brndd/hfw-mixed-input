#include "patches.hpp"
#include "scanner.hpp"
#include "logger.hpp"
#include <array>
#include <string_view>
#include <cstring>

namespace mod::patches {

bool write_nop(uint8_t* target, size_t size) {
    if (!target || size == 0) return false;
    ScopedMemoryProtect protect(target, size);
    if (!protect.is_valid()) return false;
    memset(target, 0x90, size);
    return true;
}

bool write_bytes(uint8_t* target, std::span<const uint8_t> data) {
    if (!target || data.empty()) return false;
    ScopedMemoryProtect protect(target, data.size());
    if (!protect.is_valid()) return false;
    memcpy(target, data.data(), data.size());
    return true;
}

struct PatchDef {
    std::string_view name;
    std::string_view pattern;
    size_t patch_offset;
    std::span<const uint8_t> patch_bytes;
};

static constexpr uint8_t NOP2[] = { 0x90, 0x90 };
static constexpr uint8_t NOP5[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
static constexpr uint8_t NOP6[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

static const std::array<PatchDef, 12> G_PATCHES = {{
    // 1. Combine Gamepad Stick Look and Mouse Look in FUN_141193830
    {
        .name = "Camera Look Combine Gamepad + Mouse",
        .pattern = "80 BD ?? ?? 00 00 00 48 8D 44 24 ?? C5 F8 57 C0 74 ?? C4 E3 79 21 44 24",
        .patch_offset = 16,
        .patch_bytes = LOOK_ROTATION_COMBINE_PATCH
    },
    // 2. Prevent mouse movement from switching active device in FUN_14008a950
    {
        .name = "Mouse Device Switch Suppression",
        .pattern = "C5 FA 11 86 ?? ?? 00 00 C5 FA 11 8E ?? ?? 00 00 E8 ?? ?? ?? ?? 48 8B 1E 48 8B CE",
        .patch_offset = 16,
        .patch_bytes = NOP5
    },
    // 3. Analog Sampler: Unblock Gamepad Axes (FUN_14008c110)
    {
        .name = "Analog: Unblock Gamepad Axes",
        .pattern = "83 BC 8F 9C 06 00 00 01 0F 84 79 01 00 00 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP6
    },
    // 4. Analog Sampler: Unblock Mouse Look Axes (FUN_14008c110)
    {
        .name = "Analog: Unblock Mouse Look Axes",
        .pattern = "83 BC 8F 9C 06 00 00 02 0F 84 18 01 00 00 8D 83 0C FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP6
    },
    // 5. Analog Sampler: Unblock Gamepad Buttons (FUN_14008c110)
    {
        .name = "Analog: Unblock Gamepad Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 01 0F 84 CA 00 00 00 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP6
    },
    // 6. Analog Sampler: Unblock Mouse Buttons (FUN_14008c110)
    {
        .name = "Analog: Unblock Mouse Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 6A 8D 83 D4 FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 7. Analog Sampler: Unblock Keyboard Keys (FUN_14008c110)
    {
        .name = "Analog: Unblock Keyboard Keys",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 3A 8B 8C 9F F8 00 00 00",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 8. Digital Sampler: Unblock Gamepad Sticks Threshold (FUN_14008bf00)
    {
        .name = "Digital: Unblock Gamepad Sticks",
        .pattern = "83 BC 8F 9C 06 00 00 01 74 2E 44 8B C6 8B D3",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 9. Digital Sampler: Unblock Mouse Axes Threshold (FUN_14008bf00)
    {
        .name = "Digital: Unblock Mouse Axes",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 C3 8D 83 0C FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 10. Digital Sampler: Unblock Gamepad Buttons (FUN_14008bf00)
    {
        .name = "Digital: Unblock Gamepad Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 01 74 48 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 11. Digital Sampler: Unblock Mouse Buttons (FUN_14008bf00)
    {
        .name = "Digital: Unblock Mouse Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 D0 8D 83 D4 FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2
    },
    // 12. Digital Sampler: Unblock Keyboard Keys (FUN_14008bf00)
    {
        .name = "Digital: Unblock Keyboard Keys",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 9D 8B 84 9F F8 00 00 00",
        .patch_offset = 8,
        .patch_bytes = NOP2
    }
}};

bool apply_all_patches() {
    HMODULE main_module = GetModuleHandleA(nullptr);
    if (!main_module) {
        logger::error("Failed to get main executable module handle!");
        return false;
    }

    auto text_region = scanner::get_module_section(main_module, ".text");
    if (!text_region) {
        text_region = scanner::get_module_section(main_module, "");
    }
    if (!text_region) {
        logger::error("Failed to locate executable section for patching!");
        return false;
    }

    size_t success_count = 0;
    for (size_t i = 0; i < G_PATCHES.size(); ++i) {
        const auto& p = G_PATCHES[i];
        uint8_t* match = scanner::scan(*text_region, p.pattern);
        if (!match) {
            logger::error("Patch [{}/{}] FAILED to find signature: '{}'", i + 1, G_PATCHES.size(), p.name);
            continue;
        }

        uint8_t* patch_addr = match + p.patch_offset;
        if (write_bytes(patch_addr, p.patch_bytes)) {
            success_count++;
        } else {
            logger::error("Patch [{}/{}] FAILED writing memory for '{}'", i + 1, G_PATCHES.size(), p.name);
        }
    }

    if (success_count == G_PATCHES.size()) {
        logger::debug("Successfully applied all {} patches.", G_PATCHES.size());
        return true;
    } else {
        logger::error("Patching incomplete: only {} / {} patches succeeded.", success_count, G_PATCHES.size());
        return false;
    }
}

bool apply_mouse_smoothing_patch() {
    HMODULE main_module = GetModuleHandleA(nullptr);
    if (!main_module) return false;

    auto text_region = scanner::get_module_section(main_module, ".text");
    if (!text_region) {
        text_region = scanner::get_module_section(main_module, "");
    }
    if (!text_region) return false;

    bool success = true;
    static constexpr uint8_t BYTE_ZERO[] = { 0x00 };
    static constexpr uint8_t JMP_OPCODE[] = { 0xEB };

    // 1. NxInputImpl Constructor (0x14008651D)
    // Changes 'MOV word ptr [RBX + 0x568], 0x101' -> '0x0001' (initializes EnableMouseSmoothing at +0x569 to false)
    const char* sig_ctor = "66 C7 83 68 05 00 00 01 01 40 88 BB 74 05 00 00";
    uint8_t* match_ctor = scanner::scan(*text_region, sig_ctor);
    if (match_ctor) {
        if (write_bytes(match_ctor + 8, BYTE_ZERO)) {
            logger::debug("Patched NxInputImpl constructor: EnableMouseSmoothing initialized to false (0)");
        } else {
            logger::error("Failed to write NxInputImpl constructor patch");
            success = false;
        }
    } else {
        logger::warn("Failed to find NxInputImpl constructor signature");
        success = false;
    }

    // 2. NxInputImpl::Init Property Registration Default (0x14008B3B1)
    // Changes default value passed to CoreConfig::RegisterProperty("EnableMouseSmoothing", ...) from 1 to 0
    const char* sig_prop = "4D 8D 8E 69 05 00 00 48 8B 01 48 8D 15 ?? ?? ?? ?? C6 44 24 50 01";
    uint8_t* match_prop = scanner::scan(*text_region, sig_prop);
    if (match_prop) {
        if (write_bytes(match_prop + 21, BYTE_ZERO)) {
            logger::debug("Patched EnableMouseSmoothing config property registration: default set to false (0)");
        } else {
            logger::error("Failed to write EnableMouseSmoothing property registration patch");
            success = false;
        }
    } else {
        logger::warn("Failed to find EnableMouseSmoothing property registration signature");
        success = false;
    }

    // 3. Relative Raw Mouse Smoothing Bypass in ProcessRawMouseInput (0x14008B21C)
    // Changes 'JZ 0x14008B248' (74 23) to 'JMP 0x14008B248' (EB 23)
    const char* sig_rel_smooth = "80 BB 69 05 00 00 00 74 ?? C5 E8 57 D2 C5 EA 2A 57 10";
    uint8_t* match_rel = scanner::scan(*text_region, sig_rel_smooth);
    if (match_rel) {
        if (write_bytes(match_rel + 7, JMP_OPCODE)) {
            logger::debug("Patched ProcessRawMouseInput: Relative mouse smoothing bypassed (0xEB)");
        } else {
            logger::error("Failed to write Relative mouse smoothing bypass patch");
            success = false;
        }
    } else {
        logger::warn("Failed to find Relative mouse smoothing branch signature");
        success = false;
    }

    // 4. Absolute Raw Mouse Smoothing Bypass in ProcessRawMouseInput (0x14008B2EA)
    // Changes 'JZ 0x14008B319' (74 16) to 'JMP 0x14008B319' (EB 16)
    const char* sig_abs_smooth = "80 BB 69 05 00 00 00 C5 CA 5C 83 40 05 00 00 C5 C2 5C 8B 44 05 00 00 74 ??";
    uint8_t* match_abs = scanner::scan(*text_region, sig_abs_smooth);
    if (match_abs) {
        if (write_bytes(match_abs + 23, JMP_OPCODE)) {
            logger::debug("Patched ProcessRawMouseInput: Absolute mouse smoothing bypassed (0xEB)");
        } else {
            logger::error("Failed to write Absolute mouse smoothing bypass patch");
            success = false;
        }
    } else {
        logger::warn("Failed to find Absolute mouse smoothing branch signature");
        success = false;
    }

    return success;
}

} // namespace mod::patches
