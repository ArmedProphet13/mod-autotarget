#include "Orchestration/HookRegistry.h"

#include "Diagnostics/Logger.h"
#include "GameInterface/Hooks/SelectionHook.h"
#include "GameInterface/Hooks/SpellCastHook.h"
#include "GameInterface/Hooks/TabHook.h"

namespace autotarget {

void HookRegistry::InstallLive(ITargetingOracle* oracle) {
    selectionInstalled_ = SelectionHook::Install();
    if (!selectionInstalled_)
        AT_LOG_WARN("SelectionHook unavailable - manual-hold detection disabled");

    if (oracle == nullptr)
        return;

    spellCastInstalled_ = SpellCastHook::Install(oracle);
    if (!spellCastInstalled_)
        AT_LOG_WARN("SpellCastHook unavailable - ActionTarget commits "
                    "disabled, falling back to mouseover behaviour");

    tabInstalled_ = TabHook::Install(oracle);
    if (!tabInstalled_)
        AT_LOG_WARN("TabHook unavailable - Tab keeps native "
                    "cycle-by-proximity behaviour");
}

void HookRegistry::UninstallAll() {
    if (selectionInstalled_) { SelectionHook::Uninstall(); selectionInstalled_ = false; }
    if (spellCastInstalled_) { SpellCastHook::Uninstall(); spellCastInstalled_ = false; }
    if (tabInstalled_)       { TabHook::Uninstall();       tabInstalled_       = false; }
}

} // namespace autotarget
