#include "TargetScorer.h"

#include <algorithm>

namespace autotarget {

namespace {
float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}
} // namespace

float TargetScorer::Score(Tier tier, float distance, float angleOffset,
                          Guid guid, Guid currentTarget,
                          Guid previousSoftTarget) const {
    if (tier == Tier::None)
        return 0.0f;

    const float centered  = Clamp01(1.0f - angleOffset / cfg_.tier2HalfAngleRad);
    const float closeness = Clamp01(1.0f - distance    / cfg_.tier2Range);

    // Tier1 (brawl bubble): closeness only - the angle to a point-blank unit is
    // pure atan2 noise, so we deliberately ignore it. Ties resolve by GUID sort.
    // Tier2 (aimed zone):   centred dominates; closeness is an epsilon
    // tiebreaker among equally-aimed mobs, never enough to override aim.
    float score = (tier == Tier::One)
                      ? closeness
                      : (centered + closeness * 0.01f);

    // No previous-soft stickiness: the player casts roughly every ~2 s (mage
    // cast time), so what matters is "most-centred RIGHT NOW", not stability
    // between ticks. Carrying the last pick forward would mean a freshly-aimed
    // mob loses to the previous one and the next cast goes to the wrong unit.
    // The previousSoftTarget arg is kept in the signature for symmetry; it is
    // intentionally not consulted here.
    (void)previousSoftTarget;

    // HardTarget mode keeps its hysteresis: there, the "current target" is the
    // *committed* target the player is already attacking, and we don't want
    // a marginally-better mob to displace it mid-fight.
    if (guid != kNoGuid && guid == currentTarget)
        score += cfg_.hysteresisBonus;

    return score;
}

} // namespace autotarget
