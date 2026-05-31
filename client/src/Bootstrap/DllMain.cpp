#include <windows.h>

#include "Bootstrap/Initializer.h"

// Entry point of the AutoTarget proxy DLL.
//
// The DLL is loaded by WoW.exe through the normal Windows DLL search order (it
// ships as d3d9.dll next to the executable). DllMain runs under the loader lock,
// so it does nothing heavy here — Initializer::Begin only spawns a thread.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        autotarget::Initializer::Begin(module);
        break;
    case DLL_PROCESS_DETACH:
        // reserved != nullptr => the process is terminating (the usual case for
        // a proxy DLL); nullptr => an explicit FreeLibrary unload.
        autotarget::Initializer::End(reserved != nullptr);
        break;
    }
    return TRUE;
}
