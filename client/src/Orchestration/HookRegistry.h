#pragma once

namespace autotarget {

class ITargetingOracle;
class IToggleTarget;

// Lifecycle owner for every native detour AutoTarget installs into the
// client. Replaces the procedural sequence of `XxxHook::Install(...)` calls
// that used to live in Initializer.
//
// Each Install* method is best-effort: failure logs a warning and leaves
// that hook uninstalled, but never aborts the wider install. The frame hook
// is the one exception — it has its own retry loop and is owned outside the
// registry because it gates the entire tick loop.
class HookRegistry {
public:
    // Install SelectionHook (always), and the ActionTarget hooks
    // (SpellCastHook + TabHook) when those mechanisms are wanted. Pass null
    // `oracle` to skip the ActionTarget hooks.
    void InstallLive(ITargetingOracle* oracle);

    // Uninstall everything the registry owns. Safe to call multiple times
    // and safe to call from process-termination paths.
    void UninstallAll();

private:
    bool selectionInstalled_ = false;
    bool spellCastInstalled_ = false;
    bool tabInstalled_       = false;
};

} // namespace autotarget
