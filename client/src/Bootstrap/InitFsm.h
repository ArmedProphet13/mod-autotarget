#pragma once

#include <cstdint>
#include <memory>

#include <windows.h>

#include "Config/ConfigManager.h"
#include "Orchestration/AutoTargetController.h"
#include "Orchestration/HookRegistry.h"

namespace autotarget {

// Owns the bootstrap state previously scattered across Initializer namespace
// statics (controller pointer, registry, UI-install flags, in-world timer).
// Drives the post-attach lifecycle as an explicit state machine so the
// per-frame body is a single switch instead of a chain of overlapping flags.
class InitFsm {
public:
    enum class State {
        PreInit,            // construction default; nothing installed yet
        ConfigLoaded,       // config + log level applied; mode known
        ControllerBuilt,    // controller exists; hooks installed (or skipped per mode)
        WaitingForWorld,    // frame hook live; waiting for player to enter world
        UiInstalled,        // in-game checkbox + /at attempted
        Disabled            // Mode=off or a fatal init error latched us off
    };

    // Run from the worker thread on DLL attach. Drives PreInit ->
    // ConfigLoaded -> ControllerBuilt -> WaitingForWorld. Returns false on
    // any fatal init failure (logged before return).
    bool Start(HMODULE self);

    // Run from the worker thread on DLL detach. Tears down hooks and the
    // controller, leaving the FSM in a quiescent state. processTerminating
    // mirrors Initializer::End semantics: when true, skip the destructive
    // paths because the OS is about to reclaim everything.
    void Stop(bool processTerminating);

    // Called from the frame callback. Handles the WaitingForWorld -> UiInstalled
    // transition and forwards each frame into the controller. Cheap when
    // there's nothing to do.
    void OnFrame();

    State CurrentState() const { return state_; }

    // Plain function pointer for FrameHook to call. Forwards to OnFrame on
    // the active instance via a file-static bridge in the .cpp.
    static void FrameCallback();

private:
    HMODULE         self_     = nullptr;
    State           state_    = State::PreInit;
    ConfigManager   config_;
    std::unique_ptr<AutoTargetController> controller_;
    HookRegistry    hooks_;

    bool          live_           = false;
    std::uint32_t inWorldSinceMs_ = 0;
};

} // namespace autotarget
