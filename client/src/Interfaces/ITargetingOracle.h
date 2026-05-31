#pragma once

#include "Engine/EngineTypes.h"

namespace autotarget {

// Narrow interface the cast/tab hooks depend on. Read-only view of the
// targeting state plus a single light-weight predicate. Lets hooks bind
// to a 3-method surface instead of the whole controller.
class ITargetingOracle {
public:
    virtual ~ITargetingOracle() = default;

    // True iff the controller would commit a hard target right now (Live
    // mode, master enabled, not in diagnostic).
    virtual bool CanCommit() const = 0;

    // Current soft pick (best candidate from the last tick), or kNoGuid.
    virtual Guid SoftTarget() const = 0;

    // True iff the GUID in the active target slot is unusable for a cast
    // RIGHT NOW. "Cheap" = only the LIGHT unstick reasons are evaluated
    // (Dead, Missing, OutOfRange, Evading) - safe to call synchronously
    // on the cast key-press.
    virtual bool IsActiveTargetUnusableCheap(Guid guid) = 0;
};

} // namespace autotarget
