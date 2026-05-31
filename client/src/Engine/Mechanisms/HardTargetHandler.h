#pragma once

#include "Engine/Mechanisms/IMechanismHandler.h"
#include "Interfaces/ISelectionSink.h"

namespace autotarget {

// Legacy "Mechanism = target" mode (pre-v0.2.0). Each tick may commit a
// hard target write when the engine produces SetHardTarget. Bracketed
// with ISelectionSink so the write isn't mistaken for a manual pick.
class HardTargetHandler : public IMechanismHandler {
public:
    explicit HardTargetHandler(ISelectionSink& sink) : sink_(sink) {}

    void OnFrame(MechanismCtx& /*ctx*/) override {}
    void OnTickResult(MechanismCtx& ctx,
                      const TargetingDecision& decision) override;
    const char* Name() const override { return "HardTarget"; }

private:
    ISelectionSink& sink_;
};

} // namespace autotarget
