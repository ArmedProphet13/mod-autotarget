#include "SoftTargetTracker.h"

namespace autotarget {

// Soft pick = "best candidate this very tick". No dwell, no stickiness: a mage
// presses cast roughly every 2 s and only the value in the mouseover slot at
// THAT instant matters. Carrying a previous pick forward (or making a new pick
// wait out a dwell) would mean a freshly-aimed mob isn't in the slot yet when
// the cast fires, and the spell would go to the wrong unit. We only keep the
// short grace window (a few hundred ms) to ride through a momentary LoS / cone
// dropout when the player is genuinely still aiming at the same mob.
Guid SoftTargetTracker::Update(Guid bestThisTick, std::uint32_t nowMs) {
    if (bestThisTick != kNoGuid) {
        soft_ = bestThisTick;
        lastSeenMs_ = nowMs;
        return soft_;
    }

    if (soft_ != kNoGuid && (nowMs - lastSeenMs_) <= cfg_.softTargetGraceMs)
        return soft_;

    soft_ = kNoGuid;
    return soft_;
}

void SoftTargetTracker::Reset() {
    soft_ = kNoGuid;
    lastSeenMs_ = 0;
}

} // namespace autotarget
