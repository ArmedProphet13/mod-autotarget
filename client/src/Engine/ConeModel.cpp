#include "ConeModel.h"

#include <cmath>

namespace autotarget {

namespace {

// Wrap an angle delta into [-pi, pi].
float NormalizePi(float a) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    while (a > kPi)  a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

} // namespace

ConeModel::Geometry ConeModel::Classify(const TargetingSnapshot& snap,
                                        const UnitInfo& unit) const {
    const float dx = unit.x - snap.playerX;
    const float dy = unit.y - snap.playerY;
    const float dz = unit.z - snap.playerZ;

    Geometry g{};
    g.distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Cone angle is measured in the horizontal plane against the camera yaw.
    const float angleToUnit = std::atan2(dy, dx);
    g.angleOffset = std::fabs(NormalizePi(angleToUnit - snap.cameraYaw));

    // Tier 1 (brawl zone) is range-only: at point-blank the direction vector is
    // tiny and atan2 of it is pure noise, so an angle gate there is meaningless.
    // Anything within the close range is fair game; the scorer picks the
    // closest. Tier 2 (aimed zone) keeps the cone - at longer range the angle
    // is stable and "what you are pointing at" is well defined.
    if (g.distance <= cfg_.tier1Range)
        g.tier = Tier::One;
    else if (g.distance <= cfg_.tier2Range && g.angleOffset <= cfg_.tier2HalfAngleRad)
        g.tier = Tier::Two;
    else
        g.tier = Tier::None;

    return g;
}

} // namespace autotarget
