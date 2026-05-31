#pragma once

#include "CandidateScanner.h"
#include "CommitPolicy.h"
#include "EngineTypes.h"
#include "SoftTargetTracker.h"
#include "TargetingStateMachine.h"

namespace autotarget {

// Facade that ties the engine together and owns the only cross-tick state
// (the soft target). Pure logic — no client dependencies — so it can be
// exercised directly by the off-client unit tests.
class TargetingEngine {
public:
    explicit TargetingEngine(const EngineConfig& cfg);

    // Re-tune at runtime (config reload). Resets the soft target.
    void SetConfig(const EngineConfig& cfg);

    // Run one evaluation tick.
    TargetingDecision Evaluate(const TargetingSnapshot& snap);

    // The current soft target — used by the synchronous commit-on-action path
    // when the player presses an ability with no valid hard target.
    Guid SoftTarget() const { return soft_.Current(); }

    void Reset();

private:
    EngineConfig          cfg_;
    CandidateScanner      scanner_;
    SoftTargetTracker     soft_;
    TargetingStateMachine stateMachine_;
    CommitPolicy          commit_;
};

} // namespace autotarget
