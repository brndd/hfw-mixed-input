#include "patches.h"
#include "scanner.h"
#include "logger.h"
#include <windows.h>
#include <stdint.h>

typedef struct {
    const char* name;
    const char* pattern;
    size_t patch_offset;
    const uint8_t* patch_bytes;
    size_t patch_size;
} patch_def_t;

static const uint8_t NOP2[] = { 0x90, 0x90 };
static const uint8_t NOP5[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
static const uint8_t NOP6[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

static const patch_def_t G_PATCHES[] = {
    // 1. Force native mouse camera look branch in FUN_141193830
    {
        .name = "Camera Look Native Mouse Branch",
        .pattern = "80 BD ?? ?? 00 00 00 48 8D 44 24 ?? C5 F8 57 C0 74 ?? C4 E3 79 21 44 24",
        .patch_offset = 16,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 2. Prevent mouse movement from switching active device in FUN_14008a950
    {
        .name = "Mouse Device Switch Suppression",
        .pattern = "C5 FA 11 86 ?? ?? 00 00 C5 FA 11 8E ?? ?? 00 00 E8 ?? ?? ?? ?? 48 8B 1E 48 8B CE",
        .patch_offset = 16,
        .patch_bytes = NOP5,
        .patch_size = sizeof(NOP5)
    },
    // 3. Analog Sampler: Unblock Gamepad Axes (FUN_14008c110)
    {
        .name = "Analog: Unblock Gamepad Axes",
        .pattern = "83 BC 8F 9C 06 00 00 01 0F 84 79 01 00 00 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP6,
        .patch_size = sizeof(NOP6)
    },
    // 4. Analog Sampler: Unblock Mouse Look Axes (FUN_14008c110)
    {
        .name = "Analog: Unblock Mouse Look Axes",
        .pattern = "83 BC 8F 9C 06 00 00 02 0F 84 18 01 00 00 8D 83 0C FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP6,
        .patch_size = sizeof(NOP6)
    },
    // 5. Analog Sampler: Unblock Gamepad Buttons (FUN_14008c110)
    {
        .name = "Analog: Unblock Gamepad Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 01 0F 84 CA 00 00 00 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP6,
        .patch_size = sizeof(NOP6)
    },
    // 6. Analog Sampler: Unblock Mouse Buttons (FUN_14008c110)
    {
        .name = "Analog: Unblock Mouse Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 6A 8D 83 D4 FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 7. Analog Sampler: Unblock Keyboard Keys (FUN_14008c110)
    {
        .name = "Analog: Unblock Keyboard Keys",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 3A 8B 8C 9F F8 00 00 00",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 8. Digital Sampler: Unblock Gamepad Sticks Threshold (FUN_14008bf00)
    {
        .name = "Digital: Unblock Gamepad Sticks",
        .pattern = "83 BC 8F 9C 06 00 00 01 74 2E 44 8B C6 8B D3",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 9. Digital Sampler: Unblock Mouse Axes Threshold (FUN_14008bf00)
    {
        .name = "Digital: Unblock Mouse Axes",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 C3 8D 83 0C FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 10. Digital Sampler: Unblock Gamepad Buttons (FUN_14008bf00)
    {
        .name = "Digital: Unblock Gamepad Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 01 74 48 83 BC 8F 94 06 00 00 FF",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 11. Digital Sampler: Unblock Mouse Buttons (FUN_14008bf00)
    {
        .name = "Digital: Unblock Mouse Buttons",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 D0 8D 83 D4 FE FF FF",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    },
    // 12. Digital Sampler: Unblock Keyboard Keys (FUN_14008bf00)
    {
        .name = "Digital: Unblock Keyboard Keys",
        .pattern = "83 BC 8F 9C 06 00 00 02 74 9D 8B 84 9F F8 00 00 00",
        .patch_offset = 8,
        .patch_bytes = NOP2,
        .patch_size = sizeof(NOP2)
    }
};

static bool write_memory_safe(void* target, const void* data, size_t size) {
    DWORD old_protect;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log_error("VirtualProtect failed at %p (Error: %lu)", target, GetLastError());
        return false;
    }

    memcpy(target, data, size);

    DWORD temp;
    VirtualProtect(target, size, old_protect, &temp);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return true;
}

bool apply_all_patches(void) {
    HMODULE main_module = GetModuleHandleA(NULL);
    if (!main_module) {
        log_error("Failed to get main executable module handle!");
        return false;
    }

    uint8_t* text_base = NULL;
    size_t text_size = 0;
    if (!get_module_section(main_module, ".text", &text_base, &text_size)) {
        get_module_section(main_module, NULL, &text_base, &text_size);
    }

    size_t patch_count = sizeof(G_PATCHES) / sizeof(G_PATCHES[0]);
    size_t success_count = 0;

    for (size_t i = 0; i < patch_count; ++i) {
        const patch_def_t* p = &G_PATCHES[i];
        uint8_t* match = scan_pattern(text_base, text_size, p->pattern);
        if (!match) {
            log_error("Patch [%zu/%zu] FAILED to find signature: '%s'", i + 1, patch_count, p->name);
            continue;
        }

        uint8_t* patch_address = match + p->patch_offset;
        if (write_memory_safe(patch_address, p->patch_bytes, p->patch_size)) {
            success_count++;
        } else {
            log_error("Patch [%zu/%zu] FAILED writing memory for '%s'", i + 1, patch_count, p->name);
        }
    }

    if (success_count == patch_count) {
        log_info("Successfully applied all %zu patches.", patch_count);
        return true;
    } else {
        log_error("Patching incomplete: only %zu / %zu patches succeeded.", success_count, patch_count);
        return false;
    }
}
