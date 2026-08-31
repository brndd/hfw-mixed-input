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

// 42-byte patch combining Gamepad and Mouse look in CalculateLookRotation (0x1193CE7)
// Instead of selecting one and discarding the other, vector-adds both:
//   vaddss xmm6, xmm6, dword ptr [rsp + 0x30]  (Gamepad Yaw + Mouse Yaw)
//   vaddss xmm7, xmm7, xmm15                   (Gamepad Pitch + Mouse Pitch)
//   vinsertps xmm0, xmm0, xmm6, 0x00
//   vinsertps xmm0, xmm0, xmm7, 0x20
//   vinsertps xmm0, xmm0, xmm7, 0x30
//   + 13 NOPs padding
inline constexpr uint8_t LOOK_ROTATION_COMBINE_PATCH[] = {
    0xC5, 0xCA, 0x58, 0x74, 0x24, 0x30,       // vaddss xmm6, xmm6, dword ptr [rsp + 0x30]
    0xC4, 0xC1, 0x42, 0x58, 0xFF,             // vaddss xmm7, xmm7, xmm15
    0xC4, 0xE3, 0x79, 0x21, 0xC6, 0x00,       // vinsertps xmm0, xmm0, xmm6, 0x00
    0xC4, 0xE3, 0x79, 0x21, 0xC7, 0x20,       // vinsertps xmm0, xmm0, xmm7, 0x20
    0xC4, 0xE3, 0x79, 0x21, 0xC7, 0x30,       // vinsertps xmm0, xmm0, xmm7, 0x30
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 // 13 NOPs
};

bool write_nop(uint8_t* target, size_t size);
bool write_bytes(uint8_t* target, std::span<const uint8_t> data);
bool apply_all_patches();

} // namespace mod::patches
