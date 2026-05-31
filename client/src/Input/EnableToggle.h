#pragma once

namespace autotarget {

class AutoTargetController;

// Installs the in-game on/off controls: the "Enable AutoTarget" checkbox in
// Interface Options > Combat and the /at (/autotarget) chat command. Both drive
// the controller's master enable through two argument-free native functions
// exposed to Lua.
//
// The native bridge depends on offsets::kFnFrameScriptRegisterFunction. When
// that address is unset the UI is skipped entirely - the hotkey remains the
// working toggle. See the README "Offset verification" section.
class EnableToggle {
public:
    // Runs on the game thread (frame callback), once, after FrameXML has loaded.
    // Returns true if the in-game UI (checkbox + /at command) was installed;
    // false if the native bridge offset is unset and only the hotkey is live.
    static bool Install(AutoTargetController* controller);

    // Reflects a native-side toggle (hotkey) back into the Lua UI so the
    // checkbox stays in sync. Safe to call from the frame callback.
    static void SyncToLua(bool enabled);

private:
    EnableToggle() = default;
};

} // namespace autotarget
