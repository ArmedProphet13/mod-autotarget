#pragma once

#include "Engine/SpellCommitPolicy.h" // ShouldCommitSpellTarget (re-exported)

namespace autotarget {

class ITargetingOracle;

// Hooks the client's Spell_C_CastSpell so AutoTarget can inject the engine's
// soft pick as the hard target *before* the client's no-target check runs.
//
// This is the "commit-on-action" path of Blizzard Action Targeting: the soft
// target stays invisible until the player actually casts an offensive ability,
// at which point the engine's pick becomes the real target and the cast
// resolves. Plain /cast Fireball works in ActionTarget mode; no mouseover
// macros required.
//
// Install() requires MinHook to be initialised. A wrong / mismatched function
// address makes MinHook fail and the mod silently falls back to whatever the
// configured mechanism's secondary behaviour is (typically the mouseover
// writer that v0.1 shipped with).
class SpellCastHook {
public:
    static bool Install(ITargetingOracle* oracle);
    static void Uninstall();

private:
    SpellCastHook() = default;
};

} // namespace autotarget
