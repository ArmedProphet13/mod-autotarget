#include "Orchestration/ToggleManager.h"

#include "Diagnostics/Logger.h"
#include "GameInterface/FrameScript.h"

namespace autotarget {

ToggleManager::ToggleManager(bool enabledOnStartup) : enabled_(enabledOnStartup) {}

void ToggleManager::SetEnabled(bool on) {
    if (on == enabled_)
        return;
    enabled_ = on;
    AT_LOG_INFO("AutoTarget %s", on ? "enabled" : "disabled");
    SyncToLua(on);
}

void ToggleManager::SyncToLua(bool enabled) {
    FrameScript::Execute(enabled
                             ? "if AutoTargetUpdate then AutoTargetUpdate(true) end"
                             : "if AutoTargetUpdate then AutoTargetUpdate(false) end");
}

} // namespace autotarget
