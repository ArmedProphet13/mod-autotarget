#pragma once

#include "EngineTypes.h"

namespace autotarget {

// Pure geometry: distance, angular offset from the camera aim, and the
// two-tier cone classification.
//
//   Tier One  - brawl zone:  wide cone, short range. Anything on top of you.
//   Tier Two  - aimed zone:  narrow cone, long range. You must point at it.
//
// Tier One always outranks Tier Two (enforced in TargetScorer).
class ConeModel {
public:
    explicit ConeModel(const EngineConfig& cfg) : cfg_(cfg) {}

    struct Geometry {
        float distance = 0.0f;    // 3D yards
        float angleOffset = 0.0f; // radians, absolute; 0 = dead ahead
        Tier  tier = Tier::None;
    };

    Geometry Classify(const TargetingSnapshot& snap, const UnitInfo& unit) const;

private:
    EngineConfig cfg_;
};

} // namespace autotarget
