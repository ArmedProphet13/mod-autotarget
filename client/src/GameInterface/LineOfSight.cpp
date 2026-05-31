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

// bool __cdecl TraceLine(start, end, outHitPoint, outHitFraction, flags, pCallbackData)
//
// Verified against a working 3.3.5a/12340 line-of-sight implementation
// (AzDeltaQQ/WorldToScreenTesting). Two details are mandatory or the client
// crashes: there are SIX parameters (the trailing void* callback, passed
// nullptr), and outHitPoint MUST be a valid non-null pointer - the function
// writes the hit location through it unconditionally. Passing only five args
// or a null hit pointer makes the callee read a garbage stack slot / write
// through null, which corrupts memory and faults later (e.g. on exit).
//
// Returns true when the segment is BLOCKED (hit geometry); false when CLEAR.
using IntersectFn = bool(__cdecl*)(const C3Vector* start,
                                   const C3Vector* end,
                                   C3Vector*       outHitPoint,
                                   float*          outHitFraction,
                                   unsigned int    flags,
                                   void*           pCallbackData);

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
    C3Vector hitPoint{};   // MUST be non-null: the function writes the hit here.
    float    fraction = 1.0f; // in/out: [0,1] fraction along the segment on a hit.
    // Returns true when the ray HITS geometry (blocked); the segment is clear
    // exactly when it returns false. The trailing nullptr is the optional
    // callback-data pointer (6th arg) - omitting it is what corrupted the stack.
    const bool blocked = fn(&start, &end, &hitPoint, &fraction,
                            offsets::kLosTraceFlags, nullptr);
    return !blocked;
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
