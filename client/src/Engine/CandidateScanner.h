#pragma once

#include <vector>

#include "ConeModel.h"
#include "EngineTypes.h"
#include "TargetScorer.h"

namespace autotarget {

// Filters the snapshot's units down to valid candidates and ranks them.
//
// A unit qualifies only if it is alive, attackable, not a critter, in line of
// sight, and falls inside one of the two cone tiers.
class CandidateScanner {
public:
    explicit CandidateScanner(const EngineConfig& cfg);

    // Scored candidates, best first. Empty when nothing qualifies.
    std::vector<ScoredCandidate> Scan(const TargetingSnapshot& snap) const;

    // Best candidate guid, or kNoGuid when the list is empty.
    static Guid Best(const std::vector<ScoredCandidate>& ranked);

private:
    EngineConfig cfg_;
    ConeModel    cone_;
    TargetScorer scorer_;
};

} // namespace autotarget
