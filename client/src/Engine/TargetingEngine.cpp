#include "TargetingEngine.h"

namespace autotarget {

TargetingEngine::TargetingEngine(const EngineConfig& cfg)
    : cfg_(cfg), scanner_(cfg), soft_(cfg), commit_(cfg) {}

void TargetingEngine::SetConfig(const EngineConfig& cfg) {
    cfg_ = cfg;
    scanner_ = CandidateScanner(cfg);
    soft_    = SoftTargetTracker(cfg);
    commit_  = CommitPolicy(cfg);
}

TargetingDecision TargetingEngine::Evaluate(const TargetingSnapshot& snap) {
    // Hand last tick's soft pick to the scorer so it can apply a stickiness
    // bonus. Done here (engine owns the loop) rather than in the controller so
    // the unit tests cover it for free.
    TargetingSnapshot s = snap;
    s.previousSoftTarget = soft_.Current();

    const std::vector<ScoredCandidate> ranked = scanner_.Scan(s);
    const Guid best = CandidateScanner::Best(ranked);
    const Guid softTarget = soft_.Update(best, s.nowMs);
    const TargetingStateMachine::Result state = stateMachine_.Evaluate(s);
    return commit_.Decide(s, state, softTarget);
}

void TargetingEngine::Reset() {
    soft_.Reset();
}

} // namespace autotarget
