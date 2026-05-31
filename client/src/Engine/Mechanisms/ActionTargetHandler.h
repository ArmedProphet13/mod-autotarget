#pragma once

#include "Engine/Mechanisms/IMechanismHandler.h"
#include "Interfaces/ISelectionSink.h"

namespace autotarget {

// ActionTarget mode (default). The mouseover slot mirrors the engine's
// soft pick every frame (cursor wins on hover); the active target slot is
// only written via SpellCastHook / TabHook in response to player input.
class ActionTargetHandler : public IMechanismHandler {
public:
    explicit ActionTargetHandler(ISelectionSink& sink) : sink_(sink) {}

    void OnFrame(MechanismCtx& ctx) override;
    void OnTickResult(MechanismCtx& ctx,
                      const TargetingDecision& decision) override;
    const char* Name() const override { return "ActionTarget"; }

private:
    ISelectionSink& sink_;
};

} // namespace autotarget
