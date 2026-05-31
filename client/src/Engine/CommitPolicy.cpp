#include "CommitPolicy.h"

namespace autotarget {

TargetingDecision CommitPolicy::Decide(const TargetingSnapshot& snap,
                                       const TargetingStateMachine::Result& state,
                                       Guid softTarget) const {
    TargetingDecision d{};
    d.softTarget = softTarget;
    d.hardTarget = kNoGuid;
    d.kind = DecisionKind::NoChange;

    if (softTarget == kNoGuid) {
        d.reason = "no candidate";
        return d;
    }

    if (state.state == TargetState::Void) {
        // The void: first-target acquisition and kill-to-next continuity.
        // Commit only when the player is actually fighting or has just acted,
        // so walking around with the soft target tracking stays silent.
        if (snap.inCombat || snap.actionPending) {
            d.kind = DecisionKind::SetHardTarget;
            d.hardTarget = softTarget;
            d.reason = "void fill";
        } else {
            d.reason = "void idle - soft only";
        }
        return d;
    }

    // Held: a valid hard target exists.
    if (state.manualHoldActive) {
        d.reason = "manual hold";
        return d;
    }

    if (snap.inCombat && cfg_.aggressiveReaim && softTarget != snap.currentTarget) {
        d.kind = DecisionKind::SetHardTarget;
        d.hardTarget = softTarget;
        d.reason = "aggressive reaim";
        return d;
    }

    d.reason = "held - no change";
    return d;
}

} // namespace autotarget
