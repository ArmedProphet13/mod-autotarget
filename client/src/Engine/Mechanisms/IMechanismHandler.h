#pragma once

#include "Engine/EngineTypes.h"

namespace autotarget {

// State the TickCoordinator hands to the active mechanism each frame/tick.
// `lastWrittenMouseover` is in/out: the handler updates it after each
// mouseover slot write so the cursor-yield logic can distinguish "we wrote
// this" from "the cursor wrote this".
struct MechanismCtx {
    Guid softTarget;
    Guid lastWrittenMouseover;
    PickKind softKind = PickKind::Hostile; // what softTarget is; Hostile today
    bool enabled;
    bool diagnostic;
};

// Strategy interface for the three targeting mechanisms (ActionTarget,
// Mouseover, HardTarget). The tick coordinator delegates writing decisions
// to one of these so the per-frame and per-tick paths never have to branch
// on a mechanism enum.
class IMechanismHandler {
public:
    virtual ~IMechanismHandler() = default;

    // Called every rendered frame (after SafeMode guard, before tick gate).
    // Handlers use this for high-frequency writes like the mouseover slot.
    virtual void OnFrame(MechanismCtx& ctx) = 0;

    // Called once per throttled tick after the engine has produced a
    // decision. Handlers use this for active-target commits or post-tick
    // mouseover writes.
    virtual void OnTickResult(MechanismCtx& ctx,
                              const TargetingDecision& decision) = 0;

    // Short string used by the coordinator log lines.
    virtual const char* Name() const = 0;
};

} // namespace autotarget
