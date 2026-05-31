#pragma once

#include <cstdint>

#include <memory>

#include "Config/ConfigManager.h"
#include "Engine/EngineTypes.h"
#include "Engine/Mechanisms/IMechanismHandler.h"
#include "Engine/TargetingEngine.h"
#include "GameInterface/LiveSelectionSink.h"
#include "GameInterface/ObjectManager.h"
#include "Interfaces/IToggleTarget.h"
#include "Interfaces/ITargetingOracle.h"
#include "Interfaces/IWorldState.h"

namespace autotarget {

// Owns the per-tick engine cycle.
//
// Implements ITargetingOracle (soft pick + light unstick predicate) and
// IWorldState (in-world flag). The cast/tab hooks read this; the controller
// just forwards OnFrame() into it. SafeMode brackets the tick body so a bad
// client read disables AutoTarget rather than crashing WoW.exe.
//
// Master enable + diagnostic gating come from the injected IToggleTarget +
// the diagnostic flag; the coordinator never owns the toggle itself.
class TickCoordinator : public ITargetingOracle, public IWorldState {
public:
    TickCoordinator(const ConfigManager& config, const IToggleTarget& toggle);

    void OnFrame();
    void ApplyConfig(const ConfigManager& config);

    // ITargetingOracle
    bool CanCommit() const override { return toggle_.IsEnabled() && !diagnostic_; }
    Guid SoftTarget() const override { return softTarget_; }
    bool IsActiveTargetUnusableCheap(Guid guid) override;

    // IWorldState
    bool InWorld() const override { return inWorld_; }

private:
    void Tick();
    static void TickThunk(void* ctx);

    Config              cfg_;
    TargetingEngine     engine_;
    ObjectManager       objMgr_;
    const IToggleTarget& toggle_;
    LiveSelectionSink   sink_;
    std::unique_ptr<IMechanismHandler> handler_;

    bool          diagnostic_;
    Mechanism     mechanism_;
    float         aimOffsetRad_;
    bool          ignoreCritters_;

    bool          inWorld_ = false;
    Guid          lastWrittenMouseover_ = kNoGuid;
    Guid          softTarget_ = kNoGuid;

    std::uint32_t tickIntervalMs_;
    std::uint32_t lastTickMs_ = 0;
    std::uint32_t lastDiagMs_ = 0;
};

} // namespace autotarget
