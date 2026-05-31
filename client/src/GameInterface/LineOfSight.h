#pragma once

#include "GameInterface/WoWObject.h"

namespace autotarget {

// Line-of-sight test between the local player and a unit.
//
// v1 is permissive (always returns true). The client's terrain trace routine
// has no publicly agreed 12340 signature, so wiring it is deferred. The cone
// range and angle already exclude most occluded units; a unit behind a wall but
// inside the cone is the known v1 gap. This is the designated seam for plugging
// in the verified CWorld trace later.
namespace LineOfSight {

bool Visible(const WoWUnit& from, const WoWUnit& to);

} // namespace LineOfSight
} // namespace autotarget
