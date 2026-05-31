#pragma once

#include <cstdint>

#include "Config/ConfigManager.h"
#include "Engine/EngineTypes.h"
#include "Engine/TargetingEngine.h"
#include "GameInterface/ObjectManager.h"

namespace autotarget {

// Per-tick coordinator. Driven by the FrameHook callback once per rendered
// frame; throttles itself to the configured tick rate so the engine runs at a
// steady cadence regardless of frame rate.
//
// Each tick it builds a TargetingSnapshot from the Game Interface, runs the
// pure TargetingEngine, and applies the resulting decision. The whole tick body
// runs under SafeMode, so a bad client read disables AutoTarget rather than
// crashing WoW.exe.
class AutoTargetController {
public:
    explicit AutoTargetController(const ConfigManager& config);

    // The FrameHook callback. Cheap to call every frame — does real work only
    // once the tick interval has elapsed.
    void OnFrame();

    // Master enable, flipped by the hotkey and the in-game checkbox. When
    // disabled the controller still tracks the soft target (so toggling back on
    // is instant) but never writes a hard target.
    void SetEnabled(bool on);
    bool IsEnabled() const { return enabled_; }

    // Re-apply configuration after a reload.
    void ApplyConfig(const ConfigManager& config);

    // True once the local player is in the world (as of the last tick). The
    // Bootstrap layer uses this to wait for the in-world Lua environment before
    // injecting the in-game UI.
    bool InWorld() const { return inWorld_; }

    // Current soft pick (best candidate from the last tick), or kNoGuid. Read
    // by SpellCastHook to decide what to inject on a cast.
    Guid SoftTarget() const { return softTarget_; }

    // True iff the controller would commit a hard target right now (Live mode,
    // master enabled, not in diagnostic). SpellCastHook checks this before
    // touching anything.
    bool CanCommit() const { return enabled_ && !diagnostic_; }

    // True iff the GUID in the active target slot is unusable for a cast
    // RIGHT NOW. "Cheap" = only the LIGHT unstick reasons are evaluated
    // (Dead, Missing, OutOfRange, Evading) - no snapshot needed, safe to
    // call synchronously on the cast key-press.
    //
    // Dead/Missing always evaluate. The other reasons are gated by
    // cfg_.smartUnstick so a player who flips the feature off gets v0.3.3
    // behaviour (corpse-clear only, no range/evade re-aim).
    //
    // Returns false for kNoGuid and the no-target sentinels - those are the
    // "slot already empty" case, handled by IsTargetSlotEmpty() instead.
    bool IsActiveTargetUnusableCheap(Guid guid);

private:
    void Tick();                       // one throttled evaluation
    static void TickThunk(void* ctx);  // SafeMode GuardedFn entry point

    Config          cfg_;
    TargetingEngine engine_;
    ObjectManager   objMgr_;

    bool          enabled_ = true;
    bool          diagnostic_ = true;   // read & log only, never set a target
    Mechanism     mechanism_ = Mechanism::Mouseover;
    float         aimOffsetRad_ = 0.0f; // constant aim correction
    bool          ignoreCritters_ = false;
    bool          inWorld_ = false;     // player was in the world last tick
    Guid          lastWrittenMouseover_ = kNoGuid; // last value we wrote to the
                                                   // mouseover slot. Used by the
                                                   // cursor-yield logic to tell
                                                   // "cursor wrote this" from
                                                   // "we wrote this".
    Guid          softTarget_ = kNoGuid; // last decision's soft pick (read by SpellCastHook)
    std::uint32_t tickIntervalMs_ = 70;
    std::uint32_t lastTickMs_ = 0;
    std::uint32_t lastDiagMs_ = 0;      // throttles the diagnostic log line
    std::uint32_t lastCamProbeMs_ = 0;  // throttles the camera probe dump
};

} // namespace autotarget
