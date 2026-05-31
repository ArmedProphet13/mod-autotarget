#include "GameInterface/Hooks/SelectionHook.h"

#include <cstdint>

#include <windows.h>

#include "MinHook.h"

#include "Diagnostics/Logger.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

namespace {

// Same routine AutoTarget calls via Selection::SetTarget. Calling convention is
// a VERIFY item; correct it here and in Selection.cpp together if needed.
using TargetUnitFn = void(__cdecl*)(std::uint64_t guid);

TargetUnitFn  g_original = nullptr;
volatile bool g_committing = false;
Guid          g_lastManual = kNoGuid;
void*         g_addr = nullptr;

void __cdecl HookedTargetUnit(std::uint64_t guid) {
    // A selection that is not bracketed by our own commit is the player's.
    if (!g_committing && guid != kNoGuid)
        g_lastManual = guid;
    g_original(guid);
}

} // namespace

bool SelectionHook::Install() {
    g_addr = reinterpret_cast<void*>(offsets::kFnTargetUnit);

    if (MH_CreateHook(g_addr, &HookedTargetUnit,
                      reinterpret_cast<void**>(&g_original)) != MH_OK) {
        AT_LOG_ERROR("SelectionHook: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(g_addr) != MH_OK) {
        AT_LOG_ERROR("SelectionHook: MH_EnableHook failed");
        return false;
    }

    AT_LOG_INFO("SelectionHook: installed at %p", g_addr);
    return true;
}

void SelectionHook::Uninstall() {
    g_lastManual = kNoGuid;
}

Guid SelectionHook::LastManualTarget() { return g_lastManual; }
void SelectionHook::ClearManual()      { g_lastManual = kNoGuid; }
void SelectionHook::BeginCommit()      { g_committing = true; }
void SelectionHook::EndCommit()        { g_committing = false; }

} // namespace autotarget
