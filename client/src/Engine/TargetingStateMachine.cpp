#include "TargetingStateMachine.h"

namespace autotarget {

const UnitInfo* TargetingStateMachine::Find(const TargetingSnapshot& snap, Guid guid) {
    if (guid == kNoGuid)
        return nullptr;
    for (const UnitInfo& u : snap.units)
        if (u.guid == guid)
            return &u;
    return nullptr;
}

bool TargetingStateMachine::IsValidTarget(const UnitInfo* u) {
    return u != nullptr && u->alive && u->attackable;
}

TargetingStateMachine::Result
TargetingStateMachine::Evaluate(const TargetingSnapshot& snap) const {
    Result r{};

    const UnitInfo* current = Find(snap, snap.currentTarget);
    const bool currentValid = IsValidTarget(current);

    r.state = currentValid ? TargetState::Held : TargetState::Void;

    // A manual hold counts only while the player's hand-picked unit is still
    // the current target and still valid. Once it dies or is replaced, the
    // hold has lapsed and aggressive re-aim resumes.
    r.manualHoldActive = currentValid &&
                         snap.manualHold != kNoGuid &&
                         snap.manualHold == snap.currentTarget;
    return r;
}

} // namespace autotarget
