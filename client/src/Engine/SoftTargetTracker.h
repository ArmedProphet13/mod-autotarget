#pragma once

#include "EngineTypes.h"

namespace autotarget {

// Holds the "soft" pick the engine wants to feed to the mouseover slot.
//
// Design: NO stickiness, NO dwell. The pick is just "best candidate this tick."
// The player casts ~once every 2 s (mage cast time), so what matters is the
// value in the slot at the moment of press, not stability between ticks. Any
// memory bias would mean a freshly-aimed mob isn't in the slot yet when the
// cast fires.
//
// A short grace window keeps the last pick alive across momentary cone/LoS
// dropouts so the soft target doesn't blink to nothing when you skim past a
// pillar while still aiming at the same mob.
class SoftTargetTracker {
public:
    explicit SoftTargetTracker(const EngineConfig& cfg) : cfg_(cfg) {}

    Guid Update(Guid bestThisTick, std::uint32_t nowMs);

    Guid Current() const { return soft_; }
    void Reset();

private:
    EngineConfig  cfg_;
    Guid          soft_ = kNoGuid;
    std::uint32_t lastSeenMs_ = 0;
};

} // namespace autotarget
