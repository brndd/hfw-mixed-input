#include "context_hook.h"
#include "scanner.h"
#include "logger.h"
#include "steam_input.h"
#include <windows.h>
#include <stdint.h>
#include <ctype.h>

typedef void (*EnableContext_fn)(void* this_ptr, void* context_desc);
typedef void (*DisableContext_fn)(void* this_ptr, void* context_desc);

static EnableContext_fn o_EnableContext = NULL;
static DisableContext_fn o_DisableContext = NULL;

static const char* safe_get_context_name(void* context_desc) {
    if (!context_desc) return "<NULL>";
    
    if (IsBadReadPtr(context_desc, 1)) return "<Unreadable>";

    const char* str = (const char*)context_desc;
    for (int i = 0; i < 4; ++i) {
        if (str[i] == '\0') {
            if (i == 0) return "<Empty>";
            break;
        }
        if (!isprint((unsigned char)str[i]) && str[i] != '_') {
            return "<Non-String Descriptor>";
        }
    }
    return str;
}

static void Hook_EnableContext(void* this_ptr, void* context_desc) {
    const char* name = safe_get_context_name(context_desc);
    log_info(">>> [CONTEXT ENABLED] : %s (Addr: %p)", name, context_desc);

    // Trigger SIAPI Action Set / Layer activation
    steam_input_on_context_change(name, true);

    if (o_EnableContext) {
        o_EnableContext(this_ptr, context_desc);
    }
}

static void Hook_DisableContext(void* this_ptr, void* context_desc) {
    const char* name = safe_get_context_name(context_desc);
    log_info("<<< [CONTEXT DISABLED]: %s (Addr: %p)", name, context_desc);

    // Trigger SIAPI Action Set / Layer deactivation
    steam_input_on_context_change(name, false);

    if (o_DisableContext) {
        o_DisableContext(this_ptr, context_desc);
    }
}

bool init_context_hook(void) {
    HMODULE main_module = GetModuleHandleA(NULL);
    if (!main_module) return false;

    uint8_t* text_base = NULL;
    size_t text_size = 0;
    get_module_section(main_module, ".text", &text_base, &text_size);

    // Locate NxInputImpl_UpdateGamepadActiveContext via signature
    const char* sig = "48 89 5C 24 08 57 48 83 EC 20 48 8B 01 48 8B FA 48 8B 1D";
    uint8_t* match = scan_pattern(text_base, text_size, sig);
    if (!match) {
        log_error("Failed to find signature for NxInputImpl_UpdateGamepadActiveContext to resolve g_pNxGameStateImpl!");
        return false;
    }

    uint8_t* mov_instr = match + 16;
    int32_t disp = *(int32_t*)(mov_instr + 3);
    void** pp_gameState = (void**)(mov_instr + 7 + disp);

    log_info("Resolved g_pNxGameStateImpl pointer location at %p", pp_gameState);

    // Wait until g_pNxGameStateImpl is instantiated
    int retries = 50;
    while (!*pp_gameState && retries > 0) {
        Sleep(100);
        retries--;
    }

    if (!*pp_gameState) {
        log_error("g_pNxGameStateImpl was not initialized in time!");
        return false;
    }

    void* gameState = *pp_gameState;
    void** vtable = *(void***)gameState;
    log_info("g_pNxGameStateImpl instance: %p, vtable: %p", gameState, vtable);

    // Vtable entry 15 is EnableContext (+0x78), entry 17 is DisableContext (+0x88)
    o_EnableContext = (EnableContext_fn)vtable[15];
    o_DisableContext = (DisableContext_fn)vtable[17];

    log_info("Original EnableContext: %p, DisableContext: %p", o_EnableContext, o_DisableContext);

    // Hook vtable entries
    DWORD oldProtect;
    if (VirtualProtect(&vtable[15], sizeof(void*) * 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[15] = (void*)Hook_EnableContext;
        vtable[17] = (void*)Hook_DisableContext;
        VirtualProtect(&vtable[15], sizeof(void*) * 3, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), &vtable[15], sizeof(void*) * 3);
        log_info("Successfully installed VMT hooks for EnableContext / DisableContext!");

        // Initialize SteamInput
        steam_input_init();
        return true;
    } else {
        log_error("Failed to make vtable writable (Error: %lu)", GetLastError());
        return false;
    }
}
