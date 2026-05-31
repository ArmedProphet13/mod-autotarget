#pragma once

#include "Engine/EngineTypes.h"

namespace autotarget {

// Hooks the client's target-selection routine so AutoTarget can tell a manual
// pick (the player clicked or tabbed) from its own commits.
//
// AutoTarget brackets its own Selection::SetTarget calls with BeginCommit /
// EndCommit; any selection seen outside that bracket is treated as manual and
// recorded, which is what drives the engine's manual-hold behaviour.
class SelectionHook {
public:
    // Installs the hook. Requires MinHook to be initialized.
    static bool Install();
    static void Uninstall();

    // The most recent guid the player selected by hand (kNoGuid if none).
    static Guid LastManualTarget();
    static void ClearManual();

    static void BeginCommit();
    static void EndCommit();

private:
    SelectionHook() = default;
};

} // namespace autotarget
