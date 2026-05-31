#pragma once

#include "Engine/Mechanisms/IMechanismHandler.h"
#include "Interfaces/ISelectionSink.h"

namespace autotarget {

// Legacy "Mechanism = mouseover" mode (pre-v0.3.0). Drives the mouseover
// slot unconditionally with the soft pick — no cursor yield. Kept for
// fallback compatibility.
class MouseoverHandler : public IMechanismHandler {
public:
    explicit MouseoverHandler(ISelectionSink& sink) : sink_(sink) {}

    void OnFrame(MechanismCtx& ctx) override;
    void OnTickResult(MechanismCtx& ctx,
                      const TargetingDecision& decision) override;
    const char* Name() const override { return "Mouseover"; }

private:
    ISelectionSink& sink_;
};

} // namespace autotarget
