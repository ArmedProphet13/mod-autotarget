#pragma once

#include "EngineTypes.h"

namespace autotarget {

// Turns geometry into a single comparable score.
//
// Tier1 (brawl, <=tier1Range): closeness dominates - the closest unit wins,
// angle is a near-zero tiebreaker. Tier2 (aimed, inside the cone): centred
// dominates - the most-centred unit wins, distance is a near-zero tiebreaker.
// A hysteresis bonus is applied to the previous soft pick AND to the current
// hard target so a marginally-better rival cannot flicker the pick around.
class TargetScorer {
public:
    explicit TargetScorer(const EngineConfig& cfg) : cfg_(cfg) {}

    float Score(Tier tier, float distance, float angleOffset,
                Guid guid, Guid currentTarget, Guid previousSoftTarget) const;

private:
    EngineConfig cfg_;
};

} // namespace autotarget
