#include "GameInterface/Hooks/TabHook.h"

#include <windows.h>

#include "MinHook.h"

#include "Diagnostics/Logger.h"
#include "Engine/EngineTypes.h"
#include "GameInterface/Hooks/SelectionHook.h"
#include "GameInterface/Offsets.h"
#include "GameInterface/Selection.h"
#include "Interfaces/ITargetingOracle.h"

namespace autotarget {

namespace {

// TargetNearestEnemy is a no-argument client method (thiscall on a global
// game-ui object) on 12340. We treat it as a plain no-arg __cdecl for the
// detour - on 32-bit Windows the trampoline preserves whatever convention
// the original used, and since we never call original with arguments we
// stash, the ABI mismatch is invisible.
using TabFn = void (*)();

TabFn                 g_original   = nullptr;
void*                 g_addr       = nullptr;
ITargetingOracle*     g_oracle     = nullptr;

void HookedTab() {
    if (g_oracle == nullptr || !g_oracle->CanCommit()) {
        g_original();
        return;
    }

    const Guid soft = g_oracle->SoftTarget();
    if (soft == kNoGuid) {
        // No aim pick - fall through to the native proximity cycle so Tab
        // never does *less* than it did before AutoTarget was installed.
        AT_LOG_DEBUG("tab: no soft pick, falling through to native cycle");
        g_original();
        return;
    }

    // Aim-driven swap. Bracket so SelectionHook does not record this as
    // a manual pick.
    SelectionHook::BeginCommit();
    Selection::SetTarget(soft);
    SelectionHook::EndCommit();

    AT_LOG_INFO("tab swap: -> %016llX", static_cast<unsigned long long>(soft));
    // Do NOT call g_original - we are replacing native Tab's cycle with our
    // aim-driven swap.
}

} // namespace

bool TabHook::Install(ITargetingOracle* oracle) {
    if (oracle == nullptr) {
        AT_LOG_ERROR("TabHook: null oracle");
        return false;
    }
    if (offsets::kFnTargetNearestEnemy == 0) {
        AT_LOG_WARN("TabHook: kFnTargetNearestEnemy is 0 - skipping");
        return false;
    }
    g_oracle = oracle;
    g_addr = reinterpret_cast<void*>(offsets::kFnTargetNearestEnemy);

    if (MH_CreateHook(g_addr, &HookedTab,
                      reinterpret_cast<void**>(&g_original)) != MH_OK) {
        AT_LOG_ERROR("TabHook: MH_CreateHook failed (kFnTargetNearestEnemy "
                     "may be wrong; Tab keeps native behaviour)");
        g_oracle = nullptr;
        return false;
    }
    if (MH_EnableHook(g_addr) != MH_OK) {
        AT_LOG_ERROR("TabHook: MH_EnableHook failed");
        g_oracle = nullptr;
        return false;
    }

    AT_LOG_INFO("TabHook: installed at %p (Tab = aim-driven swap)", g_addr);
    return true;
}

void TabHook::Uninstall() {
    g_oracle = nullptr;
}

} // namespace autotarget
