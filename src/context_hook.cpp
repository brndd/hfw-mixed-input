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

    auto addr = reinterpret_cast<uintptr_t>(context_desc);
    // User-mode pointer range sanity check
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return nullptr;

    const char* str = static_cast<const char*>(context_desc);
    for (int i = 0; i < 64; ++i) {
        char c = str[i];
        if (c == '\0') {
            return (i > 0) ? str : nullptr;
        }
        if (!isprint(static_cast<unsigned char>(c)) && c != '_' && c != ' ') {
            return nullptr;
        }
    }
    return nullptr;
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

    // Direct signatures for EnableContext (0x140081960) and DisableContext (0x140081A10)
    const char* sig_enable = "48 89 5C 24 08 57 48 83 EC 20 48 8B 01 48 8B FA 48 8B D9 FF 50 60 48 8B D0 48 85 C0 75 29";
    const char* sig_disable = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 01 41 0F B6 F0 48 8B FA 48 8B D9 FF 50 60";

    uint8_t* match_enable = scanner::scan(*text_region, sig_enable);
    uint8_t* match_disable = scanner::scan(*text_region, sig_disable);

    if (!match_enable || !match_disable) {
        logger::error("Failed to find signature for EnableContext / DisableContext! (enable: {:p}, disable: {:p})",
                      static_cast<void*>(match_enable), static_cast<void*>(match_disable));
        return false;
    }

    auto res_enable = safetyhook::InlineHook::create(match_enable, hook_enable_context);
    auto res_disable = safetyhook::InlineHook::create(match_disable, hook_disable_context);

    if (!res_enable || !res_disable) {
        logger::error("Failed to hook EnableContext / DisableContext via SafetyHook!");
        return false;
    }

    g_hook_enable_context = std::move(*res_enable);
    g_hook_disable_context = std::move(*res_disable);

    logger::debug("Installed SafetyHook at EnableContext ({:p}) and DisableContext ({:p})",
                  static_cast<void*>(match_enable), static_cast<void*>(match_disable));

    return true;
}

} // namespace mod::context
