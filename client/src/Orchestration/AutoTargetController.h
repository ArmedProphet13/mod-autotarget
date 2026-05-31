#pragma once

#include "Config/ConfigManager.h"
#include "Engine/EngineTypes.h"
#include "Interfaces/ITargetingOracle.h"
#include "Interfaces/IToggleTarget.h"
#include "Interfaces/IWorldState.h"
#include "Orchestration/TickCoordinator.h"
#include "Orchestration/ToggleManager.h"

namespace autotarget {

// Thin façade. Owns the ToggleManager and the TickCoordinator, and exposes
// each via its narrow interface. Bootstrap wires the FrameHook callback
// into OnFrame(); hooks bind to Oracle()/Toggle()/World() instead of this
// class so they depend on tiny surfaces, not the whole controller.
class AutoTargetController {
public:
    explicit AutoTargetController(const ConfigManager& config);

    void OnFrame() { tick_.OnFrame(); }
    void ApplyConfig(const ConfigManager& config) { tick_.ApplyConfig(config); }

    ITargetingOracle& Oracle() { return tick_; }
    IToggleTarget&    Toggle() { return toggle_; }
    IWorldState&      World()  { return tick_; }

    // Legacy convenience accessors used by Initializer logging.
    bool IsEnabled() const { return toggle_.IsEnabled(); }

private:
    ToggleManager   toggle_;
    TickCoordinator tick_;
};

} // namespace autotarget
