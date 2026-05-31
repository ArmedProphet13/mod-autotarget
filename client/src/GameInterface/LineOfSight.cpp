#include "GameInterface/LineOfSight.h"

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "GameInterface/Offsets.h"

namespace autotarget {
namespace LineOfSight {

namespace {

// Three contiguous floats, the layout the client's tracer expects for an
// endpoint. World convention: x, y horizontal, z up.
struct C3Vector {
    float x;
    float y;
    float z;
};

// char __cdecl CWorld::Intersect(start, end, hitOut, distOut, flags)
using IntersectFn = char(__cdecl*)(const C3Vector* start,
                                   const C3Vector* end,
                                   C3Vector*       hitOut,
                                   float*          distOut,
                                   unsigned int    flags);

// Eye/chest height raise (yards). Tracing foot-to-foot false-positives on
// ground slopes and the target's own model base; raising both endpoints to
// roughly chest height is what the client does and what every LoS bot does.
constexpr float kEyeHeight = 2.0f;

bool g_enabled  = false; // driven by config
bool g_faulted  = false; // sticky: a fault permanently reverts to permissive

// Performs the raw trace. Returns true if the segment is CLEAR (in LoS).
// Wrapped by Visible() in SEH; never call directly.
bool TraceClear(const WoWUnit& from, const WoWUnit& to) {
    const C3Vector start{from.X(), from.Y(), from.Z() + kEyeHeight};
    const C3Vector end{to.X(), to.Y(), to.Z() + kEyeHeight};

    const auto fn = reinterpret_cast<IntersectFn>(offsets::kFnWorldIntersect);
    float dist = 1.0f; // in/out: fraction along the segment
    // Intersect returns nonzero when the ray HITS geometry (blocked), so the
    // segment is clear exactly when it returns zero.
    const char hit = fn(&start, &end, nullptr, &dist, offsets::kLosTraceFlags);
    return hit == 0;
}

} // namespace

void SetEnabled(bool on) {
    if (on == g_enabled)
        return;
    g_enabled = on;
    AT_LOG_INFO("LineOfSight: native trace %s", on ? "enabled" : "disabled");
}

bool Visible(const WoWUnit& from, const WoWUnit& to) {
    // Permissive fallback: feature off, latched off after a fault, or either
    // unit unreadable. Matches the original v1 always-visible behaviour.
    if (!g_enabled || g_faulted || !from.Valid() || !to.Valid())
        return true;
    if (offsets::kFnWorldIntersect == 0)
        return true;

    __try {
        return TraceClear(from, to);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_faulted = true;
        AT_LOG_WARN("LineOfSight: CWorld::Intersect faulted - native trace "
                    "disabled (reverting to permissive line-of-sight)");
        return true;
    }
}

} // namespace LineOfSight
} // namespace autotarget
