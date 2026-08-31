#pragma once
#include <cstdint>
#include <span>
#include <windows.h>

namespace mod::patches {

class ScopedMemoryProtect {
public:
    ScopedMemoryProtect(void* address, size_t size, DWORD new_protect = PAGE_EXECUTE_READWRITE)
        : m_address(address), m_size(size) {
        m_success = VirtualProtect(m_address, m_size, new_protect, &m_old_protect) != 0;
    }

    ~ScopedMemoryProtect() {
        if (m_success) {
            DWORD temp;
            VirtualProtect(m_address, m_size, m_old_protect, &temp);
            FlushInstructionCache(GetCurrentProcess(), m_address, m_size);
        }
    }

    bool is_valid() const { return m_success; }

private:
    void* m_address = nullptr;
    size_t m_size = 0;
    DWORD m_old_protect = 0;
    BOOL m_success = FALSE;
};

bool write_nop(uint8_t* target, size_t size);
bool write_bytes(uint8_t* target, std::span<const uint8_t> data);
bool apply_all_patches();

} // namespace mod::patches
