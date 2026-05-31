#pragma once

#include "Interfaces/ISelectionSink.h"

namespace autotarget {

// Production ISelectionSink that forwards to the real Selection:: and
// SelectionHook:: free functions. Stateless; owned by TickCoordinator.
class LiveSelectionSink : public ISelectionSink {
public:
    Guid ReadMouseover() override;
    void WriteMouseover(Guid guid) override;
    void BeginActiveTargetCommit() override;
    void EndActiveTargetCommit() override;
    void WriteActiveTarget(Guid guid) override;
};

} // namespace autotarget
