#include "Engine/SpellCommitPolicy.h"

namespace autotarget {

bool IsTargetSlotEmpty(Guid guid) {
    // The client uses 0 for "no target" most of the time, but on some code
    // paths (notably the cast path with no selection) it passes one of two
    // all-ones sentinels. Treat all three as "empty".
    return guid == kNoGuid
        || guid == 0xFFFFFFFFFFFFFFFEull
        || guid == 0xFFFFFFFFFFFFFFFFull;
}

bool ShouldCommitSpellTarget(int /*spellId*/,
                             Guid incomingTargetGuid,
                             Guid softGuid) {
    if (softGuid == kNoGuid)
        return false;                         // nothing to commit
    if (!IsTargetSlotEmpty(incomingTargetGuid))
        return false;                         // active target wins
    return true;
}

} // namespace autotarget
