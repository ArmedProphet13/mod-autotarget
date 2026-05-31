#include "Input/EnableToggle.h"

#include <string>

#include "Diagnostics/Logger.h"
#include "GameInterface/FrameScript.h"
#include "Orchestration/AutoTargetController.h"

namespace autotarget {

namespace {

AutoTargetController* g_controller = nullptr;

// Lua C functions: argument-free by design, so no lua_to* offsets are needed.
int __cdecl LuaEnable(void* /*L*/) {
    if (g_controller)
        g_controller->SetEnabled(true);
    return 0;
}

int __cdecl LuaDisable(void* /*L*/) {
    if (g_controller)
        g_controller->SetEnabled(false);
    return 0;
}

// The checkbox + slash command. Built once, with the initial state baked in so
// the Lua side and the native side agree from the first frame.
std::string BuildUiLua(bool enabled) {
    std::string lua = "if not AutoTargetState then\n  AutoTargetState = ";
    lua += enabled ? "true" : "false";
    lua += R"LUA(
  local function apply(on)
    AutoTargetState = on
    if on then
      if AutoTarget_NativeEnable then AutoTarget_NativeEnable() end
    else
      if AutoTarget_NativeDisable then AutoTarget_NativeDisable() end
    end
  end
  function AutoTargetUpdate(on)
    AutoTargetState = on
    local cb = _G["AutoTargetEnableCheck"]
    if cb then cb:SetChecked(on) end
  end
  -- The whole UI setup is best-effort: if anything is not where we expect it
  -- (wrong panel, template, or a not-yet-ready environment) the error is
  -- swallowed so it never shows a red message in the client.
  pcall(function()
    local panel = _G["InterfaceOptionsCombatPanel"]
    if panel then
      local cb = CreateFrame("CheckButton", "AutoTargetEnableCheck", panel,
                             "InterfaceOptionsCheckButtonTemplate")
      cb:SetPoint("BOTTOMLEFT", 16, 16)
      local label = _G["AutoTargetEnableCheckText"]
      if label then label:SetText("Enable AutoTarget") end
      cb:SetScript("OnShow", function(self) self:SetChecked(AutoTargetState) end)
      cb:SetScript("OnClick", function(self) apply(self:GetChecked() and true or false) end)
    end
    if _G["SlashCmdList"] then
      _G["SLASH_AUTOTARGET1"] = "/at"
      _G["SLASH_AUTOTARGET2"] = "/autotarget"
      _G["SlashCmdList"]["AUTOTARGET"] = function(msg)
        msg = string.lower(msg or "")
        if msg == "on" then
          apply(true); AutoTargetUpdate(true)
        elseif msg == "off" then
          apply(false); AutoTargetUpdate(false)
        else
          DEFAULT_CHAT_FRAME:AddMessage("AutoTarget is " ..
            (AutoTargetState and "ON" or "OFF") .. "  (/at on | /at off)")
        end
      end
    end
    if DEFAULT_CHAT_FRAME then
      DEFAULT_CHAT_FRAME:AddMessage(
        "AutoTarget loaded - toggle in Interface Options > Combat, or /at")
    end
  end)
end
)LUA";
    return lua;
}

} // namespace

bool EnableToggle::Install(AutoTargetController* controller) {
    g_controller = controller;

    const bool bridge =
        FrameScript::RegisterFunction("AutoTarget_NativeEnable", &LuaEnable) &&
        FrameScript::RegisterFunction("AutoTarget_NativeDisable", &LuaDisable);

    // Without the native bridge the checkbox and /at command would render but
    // could not actually flip the DLL's state - clicking would do nothing.
    // Rather than ship a decorative-only control that confuses players, skip
    // the UI entirely when the bridge is unavailable. The hotkey still works.
    if (!bridge) {
        AT_LOG_WARN("EnableToggle: in-game checkbox + /at command unavailable "
                    "(FrameScript_RegisterFunction bridge failed). AutoTarget "
                    "is still running with its startup-enabled state.");
        return false;
    }

    const bool enabled = controller != nullptr && controller->IsEnabled();
    FrameScript::Execute(BuildUiLua(enabled).c_str());
    AT_LOG_INFO("EnableToggle: in-game checkbox and /at command installed");
    return true;
}

void EnableToggle::SyncToLua(bool enabled) {
    FrameScript::Execute(enabled
                             ? "if AutoTargetUpdate then AutoTargetUpdate(true) end"
                             : "if AutoTargetUpdate then AutoTargetUpdate(false) end");
}

} // namespace autotarget
