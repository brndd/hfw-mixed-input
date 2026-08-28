#include "scanner.h"
#include "logger.h"
#include <ctype.h>
#include <stdlib.h>

bool get_module_section(HMODULE module, const char* section_name, uint8_t** out_base, size_t* out_size) {
    if (!module) {
        module = GetModuleHandleA(NULL);
    }
    if (!module) return false;

    uint8_t* base = (uint8_t*)module;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    if (!section_name) {
        // Return full module size
        *out_base = base;
        *out_size = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (strncmp((char*)sec->Name, section_name, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            *out_base = base + sec->VirtualAddress;
            *out_size = sec->Misc.VirtualSize;
            return true;
        }
    }

    // Default fallback to full image
    *out_base = base;
    *out_size = nt->OptionalHeader.SizeOfImage;
    return true;
}

static int parse_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint8_t* scan_pattern(uint8_t* base, size_t size, const char* ida_pattern) {
    if (!base || size == 0 || !ida_pattern) return NULL;

    // Parse IDA pattern into bytes and mask
    size_t pat_len = 0;
    uint8_t pat_bytes[256];
    bool pat_mask[256]; // true = check, false = wildcard (??)

    const char* p = ida_pattern;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (*p == '?') {
            pat_bytes[pat_len] = 0;
            pat_mask[pat_len] = false;
            p++;
            if (*p == '?') p++;
            pat_len++;
        } else {
            int hi = parse_hex_nibble(*p++);
            if (hi == -1) break;
            while (*p == ' ') p++;
            int lo = parse_hex_nibble(*p++);
            if (lo == -1) break;

            pat_bytes[pat_len] = (uint8_t)((hi << 4) | lo);
            pat_mask[pat_len] = true;
            pat_len++;
        }
        if (pat_len >= sizeof(pat_bytes)) break;
    }

    if (pat_len == 0 || pat_len > size) return NULL;

    // Scan memory
    size_t max_idx = size - pat_len;
    for (size_t i = 0; i <= max_idx; ++i) {
        bool match = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (pat_mask[j] && base[i + j] != pat_bytes[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return base + i;
        }
    }

    return NULL;
}
