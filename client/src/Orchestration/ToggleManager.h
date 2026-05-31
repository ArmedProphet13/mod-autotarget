#pragma once

#include "Interfaces/IToggleTarget.h"

namespace autotarget {

// Owns the master on/off state. The in-game checkbox and /at slash command
// drive it through IToggleTarget; the controller's tick path reads
// IsEnabled() to decide whether to write to the active target slot.
//
// Out-of-band toggles (hotkeys, future gamepad bindings) call SyncToLua()
// after flipping so the in-game checkbox stays consistent with the native
// state.
class ToggleManager : public IToggleTarget {
public:
    explicit ToggleManager(bool enabledOnStartup);

    // IToggleTarget
    bool IsEnabled() const override { return enabled_; }
    void SetEnabled(bool on) override;

    // Push the current state back into the Lua UI. Safe from the frame
    // callback; no-op if the UI was never installed.
    static void SyncToLua(bool enabled);

private:
    bool enabled_;
};

} // namespace autotarget
