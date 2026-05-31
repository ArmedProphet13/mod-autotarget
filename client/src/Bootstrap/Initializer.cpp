#include "Bootstrap/Initializer.h"
#include "Bootstrap/InitFsm.h"

#include <windows.h>

namespace autotarget {
namespace Initializer {

namespace {

InitFsm g_fsm;

DWORD WINAPI InitThread(LPVOID self) {
    g_fsm.Start(static_cast<HMODULE>(self));
    return 0;
}

} // namespace

void Begin(HMODULE self) {
    // Pin our own module so Windows can never unmap it for the rest of the
    // process. WoW imports version.dll only for a handful of GetFileVersionInfo*
    // calls and may release its reference shortly after start; without pinning,
    // our image could be unloaded while our hook detours (which live inside
    // that image) are still installed in the client, and the next frame jumps
    // into freed memory and crashes. Pinning also means the module attaches
    // exactly once, so initialization never doubles up.
    HMODULE pinned = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCSTR>(&Begin), &pinned);

    // Belt and braces: initialize exactly once even if attach somehow repeats.
    static volatile LONG started = 0;
    if (InterlockedExchange(&started, 1) != 0)
        return;

    const HANDLE thread = CreateThread(nullptr, 0, &InitThread, self, 0, nullptr);
    if (thread != nullptr)
        CloseHandle(thread);
}

void End(bool processTerminating) {
    g_fsm.Stop(processTerminating);
}

} // namespace Initializer
} // namespace autotarget
