#include "context_hook.hpp"
#include "scanner.hpp"
#include "logger.hpp"
#include "steam_input.hpp"
#include <safetyhook.hpp>
#include <windows.h>
#include <cctype>

namespace mod::context {

static safetyhook::InlineHook g_hook_enable_context;
static safetyhook::InlineHook g_hook_disable_context;

static const char* safe_get_context_name(void* context_desc) {
    if (!context_desc) return nullptr;
    if (IsBadReadPtr(context_desc, 1)) return nullptr;

    const char* str = static_cast<const char*>(context_desc);
    for (int i = 0; i < 4; ++i) {
        if (str[i] == '\0') {
            if (i == 0) return nullptr;
            break;
        }
        if (!isprint(static_cast<unsigned char>(str[i])) && str[i] != '_') {
            return nullptr;
        }
    }
    return str;
}

static void hook_enable_context(void* this_ptr, void* context_desc) {
    const char* name = safe_get_context_name(context_desc);
    if (name) {
        logger::debug(">>> [CONTEXT ENABLED] : {}", name);
        steam::on_context_change(name, true);
    }
    g_hook_enable_context.call<void>(this_ptr, context_desc);
}

static void hook_disable_context(void* this_ptr, void* context_desc) {
    const char* name = safe_get_context_name(context_desc);
    if (name) {
        logger::debug("<<< [CONTEXT DISABLED]: {}", name);
        steam::on_context_change(name, false);
    }
    g_hook_disable_context.call<void>(this_ptr, context_desc);
}

bool init() {
    HMODULE main_module = GetModuleHandleA(nullptr);
    if (!main_module) return false;

    auto text_region = scanner::get_module_section(main_module, ".text");
    if (!text_region) return false;

    // Signature for NxInputImpl_UpdateGamepadActiveContext
    const char* sig = "48 89 5C 24 08 57 48 83 EC 20 48 8B 01 48 8B FA 48 8B 1D";
    uint8_t* match = scanner::scan(*text_region, sig);
    if (!match) {
        logger::error("Failed to find signature for NxInputImpl_UpdateGamepadActiveContext!");
        return false;
    }

    uint8_t* mov_instr = match + 16;
    int32_t disp = *reinterpret_cast<int32_t*>(mov_instr + 3);
    void** pp_gameState = reinterpret_cast<void**>(mov_instr + 7 + disp);

    int retries = 50;
    while (!*pp_gameState && retries > 0) {
        Sleep(50);
        retries--;
    }

    if (!*pp_gameState) {
        logger::error("g_pNxGameStateImpl singleton was not initialized in time.");
        return false;
    }

    void* gameState = *pp_gameState;
    void** vtable = *reinterpret_cast<void***>(gameState);

    void* target_enable = vtable[15];
    void* target_disable = vtable[17];

    auto res_enable = safetyhook::InlineHook::create(target_enable, hook_enable_context);
    auto res_disable = safetyhook::InlineHook::create(target_disable, hook_disable_context);

    if (!res_enable || !res_disable) {
        logger::error("Failed to hook EnableContext / DisableContext via SafetyHook!");
        return false;
    }

    g_hook_enable_context = std::move(*res_enable);
    g_hook_disable_context = std::move(*res_disable);

    logger::debug("Installed SafetyHook at EnableContext ({:p}) and DisableContext ({:p})", target_enable, target_disable);

    steam::init();
    return true;
}

} // namespace mod::context
