#include "Engine/UnstickPolicy.h"

#include <cmath>

namespace autotarget {

const char* ReasonName(UnstickReason r) {
    switch (r) {
        case UnstickReason::None:             return "None";
        case UnstickReason::Dead:             return "Dead";
        case UnstickReason::Missing:          return "Missing";
        case UnstickReason::OutOfRange:       return "OutOfRange";
        case UnstickReason::Evading:          return "Evading";
        case UnstickReason::BehindSoftCloser: return "BehindSoftCloser";
        case UnstickReason::OutOfLoS:         return "OutOfLoS";
    }
    return "?";
}

namespace {

// Light predicates - the ones the cast hook can call cheaply. Each returns
// kNone if the predicate does NOT fire, otherwise its reason.

UnstickReason CheckMissing(const UnstickInputs& in) {
    return in.activeExists ? UnstickReason::None : UnstickReason::Missing;
}

UnstickReason CheckDead(const UnstickInputs& in) {
    if (!in.activeExists) return UnstickReason::None;
    return in.activeAlive ? UnstickReason::None : UnstickReason::Dead;
}

UnstickReason CheckOutOfRange(const UnstickInputs& in) {
    if (!in.activeExists || !in.activeAlive) return UnstickReason::None;
    const float distSq = in.activeDx * in.activeDx
                       + in.activeDy * in.activeDy
                       + in.activeDz * in.activeDz;
    const float maxSq  = in.maxRangeYards * in.maxRangeYards;
    return distSq > maxSq ? UnstickReason::OutOfRange : UnstickReason::None;
}

UnstickReason CheckEvading(const UnstickInputs& in) {
    if (!in.activeExists || !in.activeAlive) return UnstickReason::None;
    // Alive but not attackable -> server has flipped its flags (leash, evade,
    // bind to a script). No spell will land on it; treat as unusable.
    return in.activeAttackable ? UnstickReason::None : UnstickReason::Evading;
}

// Heavy predicates - tick-only.

UnstickReason CheckBehindSoftCloser(const UnstickInputs& in) {
    if (!in.activeExists || !in.activeAlive) return UnstickReason::None;
    if (in.softPick == kNoGuid)              return UnstickReason::None;
    if (!in.softInCone)                      return UnstickReason::None;

    // Angle from player facing to active target, XY plane. >90 deg = "behind".
    const float angleToActive = std::atan2(in.activeDy, in.activeDx);
    float delta = angleToActive - in.playerFacingRad;
    // Normalise into [-pi, pi].
    while (delta >  3.14159265f) delta -= 6.28318530f;
    while (delta < -3.14159265f) delta += 6.28318530f;
    const float kHalfPi = 1.57079633f;
    if (std::fabs(delta) <= kHalfPi)
        return UnstickReason::None; // active is in the front 180-degree arc

    // Soft must be strictly closer (horizontal distance) for the swap to fire.
    // If the new soft pick is FARTHER than the current active, we'd be
    // pulling the player toward a worse target - don't.
    const float activeDistSq = in.activeDx * in.activeDx + in.activeDy * in.activeDy;
    const float softDistSq   = in.softDx   * in.softDx   + in.softDy   * in.softDy;
    if (softDistSq >= activeDistSq)
        return UnstickReason::None;

    return UnstickReason::BehindSoftCloser;
}

UnstickReason CheckOutOfLoS(const UnstickInputs& in) {
    if (!in.activeExists || !in.activeAlive) return UnstickReason::None;
    return in.activeVisibleLoS ? UnstickReason::None : UnstickReason::OutOfLoS;
}

} // namespace

UnstickReason EvaluateUnstickReasonLight(const UnstickInputs& in) {
    UnstickReason r;
    if ((r = CheckMissing(in))    != UnstickReason::None) return r;
    if ((r = CheckDead(in))       != UnstickReason::None) return r;
    if ((r = CheckOutOfRange(in)) != UnstickReason::None) return r;
    if ((r = CheckEvading(in))    != UnstickReason::None) return r;
    return UnstickReason::None;
}

UnstickReason EvaluateUnstickReason(const UnstickInputs& in) {
    const UnstickReason light = EvaluateUnstickReasonLight(in);
    if (light != UnstickReason::None) return light;

    UnstickReason r;
    if ((r = CheckBehindSoftCloser(in)) != UnstickReason::None) return r;
    if ((r = CheckOutOfLoS(in))         != UnstickReason::None) return r;
    return UnstickReason::None;
}

} // namespace autotarget
