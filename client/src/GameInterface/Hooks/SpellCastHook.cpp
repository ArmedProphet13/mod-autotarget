#include "GameInterface/Hooks/SpellCastHook.h"

#include <cstdint>

#include <windows.h>

#include "MinHook.h"

#include "Diagnostics/Logger.h"
#include "GameInterface/Hooks/SelectionHook.h"
#include "GameInterface/Offsets.h"
#include "GameInterface/Selection.h"
#include "Orchestration/AutoTargetController.h"

namespace autotarget {

namespace {

// Spell_C_CastSpell calling convention on 3.3.5a (12340). `unk` and the second
// int are passed-through opaquely. The function's return value is "did the
// cast start"; we forward whatever the trampoline returns.
using CastSpellFn = int (__cdecl*)(int spellId,
                                   int unused,
                                   std::uint64_t targetGuid,
                                   char unk);

CastSpellFn           g_original   = nullptr;
void*                 g_addr       = nullptr;
AutoTargetController* g_controller = nullptr;

int __cdecl HookedCastSpell(int spellId,
                            int unused,
                            std::uint64_t targetGuid,
                            char unk) {
    if (g_controller == nullptr || !g_controller->CanCommit())
        return g_original(spellId, unused, targetGuid, unk);

    const Guid soft = g_controller->SoftTarget();

    // If the active target slot points to a unit that's unusable RIGHT NOW
    // (corpse, despawned, out of cast range, server-leashed) treat the slot
    // as empty for the commit decision. This is the cast-time fast-path of
    // SmartUnstick - the tick-time path clears the slot proactively, but the
    // ~70ms gap between ticks can leave a stale GUID; this catches that.
    const bool unusable = g_controller->IsActiveTargetUnusableCheap(targetGuid);
    const Guid effectiveIncoming = unusable ? kNoGuid : targetGuid;

    if (!ShouldCommitSpellTarget(spellId, effectiveIncoming, soft)) {
        AT_LOG_DEBUG("cast skip: spell=%d target=%016llX%s soft=%016llX",
                     spellId,
                     static_cast<unsigned long long>(targetGuid),
                     unusable ? " (unusable->empty)" : "",
                     static_cast<unsigned long long>(soft));
        return g_original(spellId, unused, targetGuid, unk);
    }

    if (unusable) {
        AT_LOG_DEBUG("cast commit: clearing unusable %016llX, acquiring %016llX",
                     static_cast<unsigned long long>(targetGuid),
                     static_cast<unsigned long long>(soft));
    }

    // Set the hard target via the client's own routine *before* forwarding the
    // cast so that (a) the cast's no-target check sees a valid selection, and
    // (b) any post-cast behaviour (auto-attack toggle, combat log) attributes
    // the cast correctly. Bracket with BeginCommit/EndCommit so SelectionHook
    // does not mistake this for a manual pick.
    SelectionHook::BeginCommit();
    Selection::SetTarget(soft);
    SelectionHook::EndCommit();

    AT_LOG_INFO("cast commit: spell=%d acquired soft pick %016llX",
                spellId, static_cast<unsigned long long>(soft));

    return g_original(spellId, unused, soft, unk);
}

} // namespace

bool SpellCastHook::Install(AutoTargetController* controller) {
    if (controller == nullptr) {
        AT_LOG_ERROR("SpellCastHook: null controller");
        return false;
    }
    if (offsets::kFnSpellCastSpell == 0) {
        AT_LOG_WARN("SpellCastHook: kFnSpellCastSpell is 0 - skipping");
        return false;
    }
    g_controller = controller;
    g_addr = reinterpret_cast<void*>(offsets::kFnSpellCastSpell);

    if (MH_CreateHook(g_addr, &HookedCastSpell,
                      reinterpret_cast<void**>(&g_original)) != MH_OK) {
        AT_LOG_ERROR("SpellCastHook: MH_CreateHook failed");
        g_controller = nullptr;
        return false;
    }
    if (MH_EnableHook(g_addr) != MH_OK) {
        AT_LOG_ERROR("SpellCastHook: MH_EnableHook failed");
        g_controller = nullptr;
        return false;
    }

    AT_LOG_INFO("SpellCastHook: installed at %p (ActionTarget mode)", g_addr);
    return true;
}

void SpellCastHook::Uninstall() {
    g_controller = nullptr;
}

} // namespace autotarget
