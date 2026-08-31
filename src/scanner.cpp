#include "scanner.hpp"
#include <vector>
#include <sstream>
#include <iomanip>

namespace mod::scanner {

std::optional<MemoryRegion> get_module_section(HMODULE module, std::string_view section_name) {
    if (!module) return std::nullopt;

    auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return std::nullopt;

    auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const uint8_t*>(module) + dos_header->e_lfanew
    );
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;

    if (section_name.empty()) {
        return MemoryRegion{
            .base = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(module)),
            .size = nt_headers->OptionalHeader.SizeOfImage
        };
    }

    auto* section = IMAGE_FIRST_SECTION(nt_headers);
    for (WORD i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i, ++section) {
        char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
        memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);

        if (section_name == name) {
            return MemoryRegion{
                .base = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(module) + section->VirtualAddress),
                .size = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData
            };
        }
    }

    return std::nullopt;
}

uint8_t* scan(std::span<const uint8_t> memory, std::string_view pattern) {
    if (memory.empty() || pattern.empty()) return nullptr;

    std::vector<int16_t> pattern_bytes;
    std::istringstream stream{std::string(pattern)};
    std::string byte_str;

    while (stream >> byte_str) {
        if (byte_str == "?" || byte_str == "??") {
            pattern_bytes.push_back(-1);
        } else {
            pattern_bytes.push_back(static_cast<int16_t>(std::stoul(byte_str, nullptr, 16)));
        }
    }

    if (pattern_bytes.empty() || pattern_bytes.size() > memory.size()) return nullptr;

    const size_t max_idx = memory.size() - pattern_bytes.size();
    for (size_t i = 0; i <= max_idx; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern_bytes.size(); ++j) {
            if (pattern_bytes[j] != -1 && memory[i + j] != static_cast<uint8_t>(pattern_bytes[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return const_cast<uint8_t*>(memory.data() + i);
        }
    }

    return nullptr;
}

uint8_t* scan(const MemoryRegion& region, std::string_view pattern) {
    return scan(region.as_span(), pattern);
}

} // namespace mod::scanner
