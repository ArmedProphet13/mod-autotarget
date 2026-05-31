#pragma once

#include "Engine/EngineTypes.h"

namespace autotarget {

// Pure commit-decision used by SpellCastHook in ActionTarget mode. Returns
// true iff the in-flight cast should be rewritten to acquire `softGuid`.
//
// Rule (v0.2.2, matches retail Blizzard Action Targeting):
//   - There must be a soft pick to inject.
//   - The cast's incoming target slot must be empty (the player has no
//     active target). If the player already has a target - whether they
//     tab/clicked it themselves or we set it on a previous cast - we
//     respect it. Aim never overrides an active target.
//
// "Empty" means 0 or one of the client's no-target sentinels
// (0xFFFFFFFFFFFFFFFE / 0xFFFFFFFFFFFFFFFF).
//
// `spellId` stays as a parameter for diagnostic logging only.
//
// Banked for v0.3: harmful-spell flag read from the spell record so a
// healer alt's beneficial cast can never acquire a hostile soft pick.
// Today's residual edge case: heal in open space, no friendly selected,
// hostile mob in the aim cone → mob becomes target, server rejects the
// cast, player retargets manually. Harmless.
bool ShouldCommitSpellTarget(int spellId,
                             Guid incomingTargetGuid,
                             Guid softGuid);

// Is the given guid "empty" from the cast hook's point of view? Exposed for
// tick-driven re-acquire in AutoTargetController, which uses the same
// "target slot empty" definition as the cast hook.
bool IsTargetSlotEmpty(Guid guid);

} // namespace autotarget
