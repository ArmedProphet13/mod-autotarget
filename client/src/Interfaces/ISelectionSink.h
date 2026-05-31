#pragma once

#include "Engine/EngineTypes.h"

namespace autotarget {

// Narrow abstraction over the client's mouseover + active-target slots.
// Mechanism handlers depend on this instead of the free Selection:: and
// SelectionHook:: functions so they can be exercised off-client with a
// recording mock.
class ISelectionSink {
public:
    virtual ~ISelectionSink() = default;

    virtual Guid ReadMouseover() = 0;
    virtual void WriteMouseover(Guid guid) = 0;

    // Begin/End bracket an active-target write so the selection hook does
    // not record it as a manual pick. Implementations may no-op these in
    // tests.
    virtual void BeginActiveTargetCommit() = 0;
    virtual void EndActiveTargetCommit() = 0;
    virtual void WriteActiveTarget(Guid guid) = 0;
};

} // namespace autotarget
