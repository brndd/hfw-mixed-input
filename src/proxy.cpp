#include "proxy.hpp"
#include "logger.hpp"
#include <string>

namespace mod::proxy {

#define FORWARD_FUNC(name) static FARPROC o_##name = nullptr;

FORWARD_FUNC(GetFileVersionInfoA)
FORWARD_FUNC(GetFileVersionInfoByHandle)
FORWARD_FUNC(GetFileVersionInfoExA)
FORWARD_FUNC(GetFileVersionInfoExW)
FORWARD_FUNC(GetFileVersionInfoSizeA)
FORWARD_FUNC(GetFileVersionInfoSizeExA)
FORWARD_FUNC(GetFileVersionInfoSizeExW)
FORWARD_FUNC(GetFileVersionInfoSizeW)
FORWARD_FUNC(GetFileVersionInfoW)
FORWARD_FUNC(VerFindFileA)
FORWARD_FUNC(VerFindFileW)
FORWARD_FUNC(VerInstallFileA)
FORWARD_FUNC(VerInstallFileW)
FORWARD_FUNC(VerLanguageNameA)
FORWARD_FUNC(VerLanguageNameW)
FORWARD_FUNC(VerQueryValueA)
FORWARD_FUNC(VerQueryValueW)

bool init() {
    char sys_path[MAX_PATH] = {};
    UINT len = GetSystemDirectoryA(sys_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH - 16) {
        logger::error("Failed to get system directory!");
        return false;
    }
    strcat(sys_path, "\\version.dll");

    HMODULE real_dll = LoadLibraryA(sys_path);
    if (!real_dll) {
        logger::error("Failed to load real version.dll from {} (Error: {})", sys_path, GetLastError());
        return false;
    }

    logger::debug("Loaded system version.dll from: {}", sys_path);

#define BIND_FUNC(name) o_##name = GetProcAddress(real_dll, #name)

    BIND_FUNC(GetFileVersionInfoA);
    BIND_FUNC(GetFileVersionInfoByHandle);
    BIND_FUNC(GetFileVersionInfoExA);
    BIND_FUNC(GetFileVersionInfoExW);
    BIND_FUNC(GetFileVersionInfoSizeA);
    BIND_FUNC(GetFileVersionInfoSizeExA);
    BIND_FUNC(GetFileVersionInfoSizeExW);
    BIND_FUNC(GetFileVersionInfoSizeW);
    BIND_FUNC(GetFileVersionInfoW);
    BIND_FUNC(VerFindFileA);
    BIND_FUNC(VerFindFileW);
    BIND_FUNC(VerInstallFileA);
    BIND_FUNC(VerInstallFileW);
    BIND_FUNC(VerLanguageNameA);
    BIND_FUNC(VerLanguageNameW);
    BIND_FUNC(VerQueryValueA);
    BIND_FUNC(VerQueryValueW);

    return true;
}

} // namespace mod::proxy

using namespace mod::proxy;

extern "C" {

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!o_GetFileVersionInfoA) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(LPCSTR, DWORD, DWORD, LPVOID)>(o_GetFileVersionInfoA)(lptstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport) int WINAPI Proxy_GetFileVersionInfoByHandle(int hMem, LPCWSTR lpFileName, int v2, int v3) {
    if (!o_GetFileVersionInfoByHandle) return 0;
    return reinterpret_cast<int(WINAPI*)(int, LPCWSTR, int, int)>(o_GetFileVersionInfoByHandle)(hMem, lpFileName, v2, v3);
}

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!o_GetFileVersionInfoExA) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(DWORD, LPCSTR, DWORD, DWORD, LPVOID)>(o_GetFileVersionInfoExA)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!o_GetFileVersionInfoExW) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID)>(o_GetFileVersionInfoExW)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!o_GetFileVersionInfoSizeA) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(LPCSTR, LPDWORD)>(o_GetFileVersionInfoSizeA)(lptstrFilename, lpdwHandle);
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!o_GetFileVersionInfoSizeExA) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCSTR, LPDWORD)>(o_GetFileVersionInfoSizeExA)(dwFlags, lpwstrFilename, lpdwHandle);
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!o_GetFileVersionInfoSizeExW) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCWSTR, LPDWORD)>(o_GetFileVersionInfoSizeExW)(dwFlags, lpwstrFilename, lpdwHandle);
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!o_GetFileVersionInfoSizeW) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(LPCWSTR, LPDWORD)>(o_GetFileVersionInfoSizeW)(lptstrFilename, lpdwHandle);
}

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!o_GetFileVersionInfoW) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID)>(o_GetFileVersionInfoW)(lptstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir, LPSTR szCurDir, PUINT lpuCurDirLen, LPSTR szDestDir, PUINT lpuDestDirLen) {
    if (!o_VerFindFileA) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT)>(o_VerFindFileA)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir, LPWSTR szCurDir, PUINT lpuCurDirLen, LPWSTR szDestDir, PUINT lpuDestDirLen) {
    if (!o_VerFindFileW) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT)>(o_VerFindFileW)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir, LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT lpuTmpFileLen) {
    if (!o_VerInstallFileA) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT)>(o_VerInstallFileA)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir, LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT lpuTmpFileLen) {
    if (!o_VerInstallFileW) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT)>(o_VerInstallFileW)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD nSize) {
    if (!o_VerLanguageNameA) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPSTR, DWORD)>(o_VerLanguageNameA)(wLang, szLang, nSize);
}

__declspec(dllexport) DWORD WINAPI Proxy_VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD nSize) {
    if (!o_VerLanguageNameW) return 0;
    return reinterpret_cast<DWORD(WINAPI*)(DWORD, LPWSTR, DWORD)>(o_VerLanguageNameW)(wLang, szLang, nSize);
}

__declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    if (!o_VerQueryValueA) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(LPCVOID, LPCSTR, LPVOID*, PUINT)>(o_VerQueryValueA)(pBlock, lpSubBlock, lplpBuffer, puLen);
}

__declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    if (!o_VerQueryValueW) return FALSE;
    return reinterpret_cast<BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT)>(o_VerQueryValueW)(pBlock, lpSubBlock, lplpBuffer, puLen);
}

} // extern "C"
