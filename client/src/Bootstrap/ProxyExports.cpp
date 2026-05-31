// version.dll proxy exports.
//
// AutoTarget ships as `version.dll` in the WoW folder so the loader picks it
// up at startup (WoW.exe statically imports version.dll for the
// GetFileVersionInfo family). WoW and Windows still expect the real version.dll
// entry points, so every export of version.dll is reproduced here and
// forwarded, at call time, to a real implementation.
//
// Forwarding target, in priority order:
//   1. `version_chain.dll` in the WoW folder    - an explicit chained wrapper.
//   2. `version.dll.disabled` in the WoW folder - the original file the player
//      renamed aside to install this proxy.
//   3. the system version.dll.
//
// Unlike d3d9.dll, version.dll is almost never replaced by graphics wrappers
// or addons, so the chained-wrapper path is a rare fallback rather than the
// common case. It is still required for correctness when present.
//
// The export names are undecorated by Bootstrap/version.def.

#include <windows.h>

namespace {

struct RealVersion {
    HMODULE module = nullptr;
    FARPROC GetFileVersionInfoA = nullptr;
    FARPROC GetFileVersionInfoW = nullptr;
    FARPROC GetFileVersionInfoSizeA = nullptr;
    FARPROC GetFileVersionInfoSizeW = nullptr;
    FARPROC GetFileVersionInfoExA = nullptr;
    FARPROC GetFileVersionInfoExW = nullptr;
    FARPROC GetFileVersionInfoSizeExA = nullptr;
    FARPROC GetFileVersionInfoSizeExW = nullptr;
    FARPROC VerQueryValueA = nullptr;
    FARPROC VerQueryValueW = nullptr;
    FARPROC VerFindFileA = nullptr;
    FARPROC VerFindFileW = nullptr;
    FARPROC VerInstallFileA = nullptr;
    FARPROC VerInstallFileW = nullptr;
    FARPROC VerLanguageNameA = nullptr;
    FARPROC VerLanguageNameW = nullptr;
};

RealVersion g_real;
bool        g_loaded = false;

// Directory this proxy DLL lives in, with a trailing backslash. Empty on
// failure. Resolved from an address inside this module so it needs no module
// handle passed in.
void ProxyDirectory(char* out, DWORD outSize) {
    out[0] = '\0';
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&ProxyDirectory), &self)) {
        return;
    }
    const DWORD n = GetModuleFileNameA(self, out, outSize);
    if (n == 0 || n >= outSize) {
        out[0] = '\0';
        return;
    }
    char* slash = nullptr;
    for (char* p = out; *p; ++p)
        if (*p == '\\' || *p == '/')
            slash = p;
    if (slash)
        *(slash + 1) = '\0'; // keep the trailing backslash
    else
        out[0] = '\0';
}

void EnsureLoaded() {
    if (g_loaded)
        return;
    g_loaded = true; // attempt once; a failed load leaves the pointers null

    // 1 & 2: a wrapper chained next to us in the WoW folder.
    char dir[MAX_PATH] = {0};
    ProxyDirectory(dir, MAX_PATH);
    if (dir[0] != '\0') {
        const char* names[] = {"version_chain.dll", "version.dll.disabled"};
        for (const char* name : names) {
            char path[MAX_PATH * 2] = {0};
            lstrcpyA(path, dir);
            lstrcatA(path, name);
            g_real.module = LoadLibraryA(path);
            if (g_real.module != nullptr)
                break;
        }
    }

    // 3: fall back to the system version.dll.
    if (g_real.module == nullptr) {
        char sys[MAX_PATH] = {0};
        const UINT n = GetSystemDirectoryA(sys, MAX_PATH);
        if (n != 0 && n <= MAX_PATH - 16) {
            lstrcatA(sys, "\\version.dll");
            g_real.module = LoadLibraryA(sys);
        }
    }

    if (g_real.module == nullptr)
        return;

    const HMODULE m = g_real.module;
    g_real.GetFileVersionInfoA        = GetProcAddress(m, "GetFileVersionInfoA");
    g_real.GetFileVersionInfoW        = GetProcAddress(m, "GetFileVersionInfoW");
    g_real.GetFileVersionInfoSizeA    = GetProcAddress(m, "GetFileVersionInfoSizeA");
    g_real.GetFileVersionInfoSizeW    = GetProcAddress(m, "GetFileVersionInfoSizeW");
    g_real.GetFileVersionInfoExA      = GetProcAddress(m, "GetFileVersionInfoExA");
    g_real.GetFileVersionInfoExW      = GetProcAddress(m, "GetFileVersionInfoExW");
    g_real.GetFileVersionInfoSizeExA  = GetProcAddress(m, "GetFileVersionInfoSizeExA");
    g_real.GetFileVersionInfoSizeExW  = GetProcAddress(m, "GetFileVersionInfoSizeExW");
    g_real.VerQueryValueA             = GetProcAddress(m, "VerQueryValueA");
    g_real.VerQueryValueW             = GetProcAddress(m, "VerQueryValueW");
    g_real.VerFindFileA               = GetProcAddress(m, "VerFindFileA");
    g_real.VerFindFileW               = GetProcAddress(m, "VerFindFileW");
    g_real.VerInstallFileA            = GetProcAddress(m, "VerInstallFileA");
    g_real.VerInstallFileW            = GetProcAddress(m, "VerInstallFileW");
    g_real.VerLanguageNameA           = GetProcAddress(m, "VerLanguageNameA");
    g_real.VerLanguageNameW           = GetProcAddress(m, "VerLanguageNameW");
}

} // namespace

// Every export below tail-calls into the real version.dll. Signatures and
// calling conventions match the Windows SDK declarations in <winver.h>.
extern "C" {

BOOL __stdcall GetFileVersionInfoA(LPCSTR file, DWORD handle, DWORD len, LPVOID data) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoA == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(LPCSTR, DWORD, DWORD, LPVOID);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoA)(file, handle, len, data);
}

BOOL __stdcall GetFileVersionInfoW(LPCWSTR file, DWORD handle, DWORD len, LPVOID data) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoW == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(LPCWSTR, DWORD, DWORD, LPVOID);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoW)(file, handle, len, data);
}

DWORD __stdcall GetFileVersionInfoSizeA(LPCSTR file, LPDWORD handle) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoSizeA == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(LPCSTR, LPDWORD);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoSizeA)(file, handle);
}

DWORD __stdcall GetFileVersionInfoSizeW(LPCWSTR file, LPDWORD handle) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoSizeW == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(LPCWSTR, LPDWORD);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoSizeW)(file, handle);
}

BOOL __stdcall GetFileVersionInfoExA(DWORD flags, LPCSTR file, DWORD handle, DWORD len, LPVOID data) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoExA == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoExA)(flags, file, handle, len, data);
}

BOOL __stdcall GetFileVersionInfoExW(DWORD flags, LPCWSTR file, DWORD handle, DWORD len, LPVOID data) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoExW == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoExW)(flags, file, handle, len, data);
}

DWORD __stdcall GetFileVersionInfoSizeExA(DWORD flags, LPCSTR file, LPDWORD handle) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoSizeExA == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCSTR, LPDWORD);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoSizeExA)(flags, file, handle);
}

DWORD __stdcall GetFileVersionInfoSizeExW(DWORD flags, LPCWSTR file, LPDWORD handle) {
    EnsureLoaded();
    if (g_real.GetFileVersionInfoSizeExW == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCWSTR, LPDWORD);
    return reinterpret_cast<Fn>(g_real.GetFileVersionInfoSizeExW)(flags, file, handle);
}

BOOL __stdcall VerQueryValueA(LPCVOID block, LPCSTR sub, LPVOID* buf, PUINT len) {
    EnsureLoaded();
    if (g_real.VerQueryValueA == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    return reinterpret_cast<Fn>(g_real.VerQueryValueA)(block, sub, buf, len);
}

BOOL __stdcall VerQueryValueW(LPCVOID block, LPCWSTR sub, LPVOID* buf, PUINT len) {
    EnsureLoaded();
    if (g_real.VerQueryValueW == nullptr) return FALSE;
    using Fn = BOOL(__stdcall*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    return reinterpret_cast<Fn>(g_real.VerQueryValueW)(block, sub, buf, len);
}

DWORD __stdcall VerFindFileA(DWORD flags, LPCSTR fn, LPCSTR wd, LPCSTR ad, LPSTR cf, PUINT cflen, LPSTR df, PUINT dflen) {
    EnsureLoaded();
    if (g_real.VerFindFileA == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
    return reinterpret_cast<Fn>(g_real.VerFindFileA)(flags, fn, wd, ad, cf, cflen, df, dflen);
}

DWORD __stdcall VerFindFileW(DWORD flags, LPCWSTR fn, LPCWSTR wd, LPCWSTR ad, LPWSTR cf, PUINT cflen, LPWSTR df, PUINT dflen) {
    EnsureLoaded();
    if (g_real.VerFindFileW == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    return reinterpret_cast<Fn>(g_real.VerFindFileW)(flags, fn, wd, ad, cf, cflen, df, dflen);
}

DWORD __stdcall VerInstallFileA(DWORD flags, LPCSTR srcFn, LPCSTR dstFn, LPCSTR srcDir, LPCSTR dstDir, LPCSTR cur, LPSTR tmp, PUINT tmpLen) {
    EnsureLoaded();
    if (g_real.VerInstallFileA == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
    return reinterpret_cast<Fn>(g_real.VerInstallFileA)(flags, srcFn, dstFn, srcDir, dstDir, cur, tmp, tmpLen);
}

DWORD __stdcall VerInstallFileW(DWORD flags, LPCWSTR srcFn, LPCWSTR dstFn, LPCWSTR srcDir, LPCWSTR dstDir, LPCWSTR cur, LPWSTR tmp, PUINT tmpLen) {
    EnsureLoaded();
    if (g_real.VerInstallFileW == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
    return reinterpret_cast<Fn>(g_real.VerInstallFileW)(flags, srcFn, dstFn, srcDir, dstDir, cur, tmp, tmpLen);
}

DWORD __stdcall VerLanguageNameA(DWORD lang, LPSTR buf, DWORD bufLen) {
    EnsureLoaded();
    if (g_real.VerLanguageNameA == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPSTR, DWORD);
    return reinterpret_cast<Fn>(g_real.VerLanguageNameA)(lang, buf, bufLen);
}

DWORD __stdcall VerLanguageNameW(DWORD lang, LPWSTR buf, DWORD bufLen) {
    EnsureLoaded();
    if (g_real.VerLanguageNameW == nullptr) return 0;
    using Fn = DWORD(__stdcall*)(DWORD, LPWSTR, DWORD);
    return reinterpret_cast<Fn>(g_real.VerLanguageNameW)(lang, buf, bufLen);
}

} // extern "C"
