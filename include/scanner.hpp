#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <optional>
#include <windows.h>

namespace mod::scanner {

struct MemoryRegion {
    uint8_t* base = nullptr;
    size_t size = 0;

    std::span<const uint8_t> as_span() const {
        return {base, size};
    }
};

std::optional<MemoryRegion> get_module_section(HMODULE module, std::string_view section_name = ".text");
uint8_t* scan(std::span<const uint8_t> memory, std::string_view pattern);
uint8_t* scan(const MemoryRegion& region, std::string_view pattern);

} // namespace mod::scanner
