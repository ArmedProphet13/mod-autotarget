#pragma once

namespace autotarget {

class ITargetingOracle;

// Hooks the client's native Tab handler (TargetNearestEnemy).
//
// Native behaviour: Tab cycles the player's target through nearby enemies
// by client-side proximity. AutoTarget replaces that with "target whatever
// the engine is aiming at" so the visible mouseover-ring preview becomes
// the actual selection on Tab press. Aim-driven swap, no cycle.
//
// If the engine has no soft pick (player facing empty space), the hook
// falls through to the native cycle. The native behaviour is preserved as
// a fallback so Tab never does *less* than it did before AutoTarget was
// installed.
//
// If MinHook fails to install (wrong offset, unpatchable function) the
// hook silently does nothing - Tab keeps its native cycle behaviour.
class TabHook {
public:
    static bool Install(ITargetingOracle* oracle);
    static void Uninstall();

private:
    TabHook() = default;
};

} // namespace autotarget
