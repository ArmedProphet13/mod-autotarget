#pragma once

#include "EngineTypes.h"

namespace autotarget {

enum class TargetState {
    Void,  // no valid hard target (none / dead / gone / unattackable)
    Held   // a valid hard target is selected
};

// Resolves the Void/Held state and tracks whether the current target is a
// deliberate manual pick that must be respected (manual hold).
class TargetingStateMachine {
public:
    struct Result {
        TargetState state = TargetState::Void;
        bool manualHoldActive = false;
    };

    Result Evaluate(const TargetingSnapshot& snap) const;

private:
    static const UnitInfo* Find(const TargetingSnapshot& snap, Guid guid);
    static bool IsValidTarget(const UnitInfo* u);
};

} // namespace autotarget
