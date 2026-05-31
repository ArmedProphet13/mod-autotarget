#pragma once

#include "GameInterface/WoWObject.h"

namespace autotarget {

// Line-of-sight test between two units, backed by the client's own terrain /
// WMO / doodad ray tracer (CWorld::Intersect, see Offsets.h).
//
// Disabled by default: SetEnabled(true) is driven by the `LineOfSightChecks`
// config flag so the single-source trace offset + flag bitmask can be verified
// in-client before being trusted. While disabled (or after a fault latches it
// off) Visible() is permissive and always returns true, exactly as the v1
// no-op did - the cone range/angle already exclude most occluded units.
namespace LineOfSight {

// Toggle the native trace on/off. Driven by config at startup and on reload.
void SetEnabled(bool on);

// True if `to` is visible from `from`. Permissive (always true) when disabled
// or after the trace call has latched off on a fault.
bool Visible(const WoWUnit& from, const WoWUnit& to);

} // namespace LineOfSight
} // namespace autotarget
