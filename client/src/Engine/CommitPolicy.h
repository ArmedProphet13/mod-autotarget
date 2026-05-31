#pragma once

#include "EngineTypes.h"
#include "TargetingStateMachine.h"

namespace autotarget {

// The final decision: given the resolved state and the current soft target,
// decide whether to write the player's hard target.
//
//   Void  + (in combat or action pending) -> commit the soft target.
//   Void  + idle                          -> no change (soft tracked silently).
//   Held  + manual hold                   -> no change (respect the choice).
//   Held  + in combat + aggressive re-aim -> follow the soft target.
//   Held  + otherwise                     -> no change.
class CommitPolicy {
public:
    explicit CommitPolicy(const EngineConfig& cfg) : cfg_(cfg) {}

    TargetingDecision Decide(const TargetingSnapshot& snap,
                             const TargetingStateMachine::Result& state,
                             Guid softTarget) const;

private:
    EngineConfig cfg_;
};

} // namespace autotarget
